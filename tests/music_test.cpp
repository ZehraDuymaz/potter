// Unit tests for the pure music logic: input handling, yt-dlp output, mpv
// messages and favorites. Same style as timer_test.cpp.

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "core/music.hpp"

using namespace potter;

static int failures = 0;

#define CHECK(cond)                                                        \
  do {                                                                     \
    if (!(cond)) {                                                         \
      std::printf("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                          \
    }                                                                      \
  } while (0)

static void classifies_input_like_play() {
  CHECK(classify_input("lofi hip hop") == InputKind::Search);
  CHECK(classify_input("  nujabes feather  ") == InputKind::Search);
  CHECK(classify_input("https://youtu.be/abc") == InputKind::Url);
  CHECK(classify_input("ytdl://ytsearch:lofi") == InputKind::Url);
  CHECK(classify_input("~/Music/song.mp3") == InputKind::File);
  CHECK(classify_input("/srv/music") == InputKind::File);
  CHECK(classify_input("./track.flac") == InputKind::File);
  CHECK(classify_input("ac/dc") == InputKind::Search);  // a band, not a path
}

static void expands_home() {
  setenv("HOME", "/home/test", 1);
  CHECK(expand_home("~/Music") == "/home/test/Music");
  CHECK(expand_home("~") == "/home/test");
  CHECK(expand_home("/etc/~x") == "/etc/~x");
}

static void parses_yt_dlp_lines() {
  const std::string sep(1, kSearchFieldSep);
  // Real lines from `yt-dlp --flat-playlist --print <template> ytsearch5:...`.
  auto r = parse_search_line("rFZHOHl-L8A" + sep + "lofi hip hop radio 📚 beats to relax/study to" +
                             sep + "Lofi Girl" + sep + "NA");
  CHECK(r.has_value());
  CHECK(r->id == "rFZHOHl-L8A");
  CHECK(r->title == "lofi hip hop radio 📚 beats to relax/study to");
  CHECK(r->channel == "Lofi Girl");
  CHECK(!r->duration_seconds);  // live stream
  CHECK(r->url() == "https://youtu.be/rFZHOHl-L8A");

  auto song = parse_search_line("x1" + sep + "Nujabes - Feather" + sep + "NA" + sep + "176");
  CHECK(song && song->duration_seconds == 176);
  CHECK(song->channel.empty());  // "NA" means unknown

  CHECK(!parse_search_line(""));
  CHECK(!parse_search_line("only one field"));
  CHECK(!parse_search_line(sep + "no id" + sep + "x" + sep + "1"));
}

static void formats_clock() {
  CHECK(format_clock(0) == "0:00");
  CHECK(format_clock(65.9) == "1:05");
  CHECK(format_clock(3723) == "1:02:03");
  CHECK(format_clock(-5) == "0:00");
}

static void follows_mpv_messages() {
  PlayerState s;
  // Lines captured from a real mpv session, in order.
  const char* lines[] = {
      R"({"request_id":0,"error":"success"})",
      R"({"event":"property-change","id":2,"name":"pause","data":false})",
      R"({"event":"property-change","id":3,"name":"volume","data":70.000000})",
      R"({"event":"start-file","playlist_entry_id":1})",
      R"({"event":"property-change","id":1,"name":"media-title","data":"silence.wav"})",
      R"({"event":"property-change","id":6,"name":"idle-active","data":false})",
      R"({"event":"property-change","id":7,"name":"path","data":"/tmp/silence.wav"})",
      R"({"event":"file-loaded"})",
      R"({"event":"property-change","id":4,"name":"time-pos","data":0.5})",
      R"({"event":"property-change","id":5,"name":"duration","data":3.000000})",
  };
  s.connected = true;
  for (const char* line : lines) apply_mpv_message(s, line);
  CHECK(s.title == "silence.wav");
  CHECK(s.path == "/tmp/silence.wav");
  CHECK(!s.idle);
  CHECK(s.playing());
  CHECK(s.volume == 70);
  CHECK(s.time_pos == 0.5);
  CHECK(s.duration == 3.0);
  CHECK(s.files_loaded == 1);

  apply_mpv_message(s, R"({"event":"property-change","id":2,"name":"pause","data":true})");
  CHECK(s.paused);
  CHECK(!s.playing());
  CHECK(s.has_media());

  // A file that fails to load.
  apply_mpv_message(s, R"({"event":"property-change","id":4,"name":"time-pos"})");
  apply_mpv_message(s, R"({"event":"start-file","playlist_entry_id":2})");
  apply_mpv_message(s, R"({"event":"end-file","reason":"error","playlist_entry_id":2,"file_error":"loading failed"})");
  apply_mpv_message(s, R"({"event":"property-change","id":6,"name":"idle-active","data":true})");
  CHECK(!s.time_pos);
  CHECK(s.idle);
  CHECK(s.error == "couldn't play this: loading failed");
  apply_mpv_message(s, R"({"event":"start-file","playlist_entry_id":3})");
  CHECK(s.error.empty());  // a new file clears the old error

  // A new file forgets the old one's position and length.
  apply_mpv_message(s, R"({"event":"property-change","id":4,"name":"time-pos","data":42.0})");
  apply_mpv_message(s, R"({"event":"property-change","id":5,"name":"duration","data":300.0})");
  apply_mpv_message(s, R"({"event":"start-file","playlist_entry_id":4})");
  CHECK(!s.time_pos);
  CHECK(!s.duration);

  // Garbage and unrelated events are ignored.
  apply_mpv_message(s, "not json at all");
  apply_mpv_message(s, R"({"event":"audio-reconfig"})");
  CHECK(s.idle);
}

static void manages_favorites() {
  const auto dir = std::filesystem::temp_directory_path() / "potter-music-test";
  std::filesystem::remove_all(dir);
  const auto path = dir / "favorites.toml";

  Favorites f;
  CHECK(f.load(path));  // missing file = empty
  CHECK(f.add("Lofi Girl radio", "https://youtu.be/jfKfPfyJRdk"));
  CHECK(!f.add("again", "https://youtu.be/jfKfPfyJRdk"));  // no duplicates
  CHECK(f.add("", "/home/test/Music/song.mp3"));             // title falls back to target
  CHECK(f.list()[1].title == "/home/test/Music/song.mp3");
  CHECK(f.contains("https://youtu.be/jfKfPfyJRdk"));
  CHECK(f.dirty());
  CHECK(f.save(path));
  CHECK(!f.dirty());

  Favorites loaded;
  CHECK(loaded.load(path));
  CHECK(loaded.list().size() == 2);
  CHECK(loaded.list()[0].title == "Lofi Girl radio");
  CHECK(loaded.remove_target("https://youtu.be/jfKfPfyJRdk"));
  CHECK(!loaded.remove_target("nope"));
  CHECK(loaded.remove(0));
  CHECK(!loaded.remove(0));
  CHECK(loaded.list().empty());

  std::ofstream(path) << "favorite = [";
  std::string error;
  CHECK(!loaded.load(path, &error));
  CHECK(!error.empty());
  std::filesystem::remove_all(dir);
}

static void makes_track_keys() {
  const std::string id = "youtube:jfKfPfyJRdk";
  CHECK(track_key("https://youtu.be/jfKfPfyJRdk") == id);
  CHECK(track_key("https://www.youtube.com/watch?v=jfKfPfyJRdk") == id);
  CHECK(track_key("https://youtube.com/watch?list=PL1&v=jfKfPfyJRdk&t=30") == id);
  CHECK(track_key("http://m.youtube.com/watch?v=jfKfPfyJRdk") == id);
  CHECK(track_key("https://music.youtube.com/watch?v=jfKfPfyJRdk") == id);
  CHECK(track_key("https://www.youtube.com/shorts/jfKfPfyJRdk") == id);
  CHECK(track_key("https://youtu.be/jfKfPfyJRdk?si=abc") == id);
  // Not a video id, or not YouTube: kept as is.
  CHECK(track_key("https://youtu.be/short") == "https://youtu.be/short");
  CHECK(track_key("https://www.youtube.com/playlist?list=PL1") ==
        "https://www.youtube.com/playlist?list=PL1");
  CHECK(track_key("https://soundcloud.com/a/b") == "https://soundcloud.com/a/b");
  CHECK(track_key("/home/test/Music/song.mp3") == "/home/test/Music/song.mp3");
}

static void remembers_positions() {
  const auto dir = std::filesystem::temp_directory_path() / "potter-resume-test";
  std::filesystem::remove_all(dir);
  const auto path = dir / "positions.toml";

  ResumePoints r;
  CHECK(r.load(path));  // missing file = empty
  r.record("https://youtu.be/jfKfPfyJRdk", 151.7, 600);
  CHECK(r.get("https://youtu.be/jfKfPfyJRdk") == 151);
  // The same video under another link.
  CHECK(r.get("https://www.youtube.com/watch?v=jfKfPfyJRdk") == 151);
  CHECK(r.dirty());

  // Near the start or the end: nothing worth resuming, and it's forgotten.
  r.record("https://youtu.be/jfKfPfyJRdk", 590, 600);
  CHECK(!r.get("https://youtu.be/jfKfPfyJRdk"));
  r.record("/music/a.mp3", 10, 600);
  CHECK(!r.get("/music/a.mp3"));
  // Live streams and short tracks aren't remembered.
  r.record("https://youtu.be/AAAAAAAAAAA", 120, std::nullopt);
  CHECK(!r.get("https://youtu.be/AAAAAAAAAAA"));
  r.record("/music/short.mp3", 20, 40);
  CHECK(!r.get("/music/short.mp3"));

  r.record("/music/a.mp3", 100, 600);
  r.record("/music/b.mp3", 200, 600);
  CHECK(r.save(path));
  CHECK(!r.dirty());
  r.record("/music/b.mp3", 200.9, 600);  // same second: nothing new
  CHECK(!r.dirty());

  ResumePoints loaded;
  CHECK(loaded.load(path));
  CHECK(loaded.get("/music/a.mp3") == 100);
  CHECK(loaded.get("/music/b.mp3") == 200);

  // Only the newest entries are kept.
  for (std::size_t i = 0; i < ResumePoints::kMaxEntries + 5; ++i) {
    loaded.record("/music/" + std::to_string(i) + ".mp3", 100, 600);
  }
  CHECK(!loaded.get("/music/a.mp3"));
  CHECK(loaded.get("/music/0.mp3") == std::nullopt);
  CHECK(loaded.get("/music/204.mp3") == 100);

  std::ofstream(path) << "track = [";
  std::string error;
  CHECK(!loaded.load(path, &error));
  CHECK(!error.empty());
  std::filesystem::remove_all(dir);
}

int main() {
  classifies_input_like_play();
  expands_home();
  parses_yt_dlp_lines();
  formats_clock();
  follows_mpv_messages();
  manages_favorites();
  makes_track_keys();
  remembers_positions();

  if (failures == 0) std::printf("all music tests passed\n");
  return failures == 0 ? 0 : 1;
}
