#include <array>
#include <chrono>
#include <cpr/cpr.h>
#include <ctime>
#include <functional>
#include <iostream>
#include <sqlite3.h>
#include <string>
#include <unordered_map>

struct ObservationData {
  std::string station_id;
  std::string location;
  std::string observation_time;
  std::string observation_time_rfc822;
  std::string weather;
  std::string wind_dir;
  std::string latitude;
  std::string longitude;
  std::string temp_f;
  std::string temp_c;
  std::string relative_humidity;
  std::string wind_degrees;
  std::string wind_mph;
  std::string wind_gust_mph;
  std::string wind_kt;
  std::string wind_gust_kt;
  std::string pressure_mb;
  std::string pressure_in;
  std::string dewpoint_f;
  std::string dewpoint_c;
  std::string visibility_mi;
  std::string abbrev;
  std::string obshash;
};

cpr::Response makeReq(std::string &url) {
  cpr::Response r = cpr::Get(
      cpr::Url{url}, cpr::Authentication{"user", "pass", cpr::AuthMode::BASIC},
      cpr::Parameters{{"anon", "true"}, {"key", "value"}});

  return r;
}

std::string getTag(const std::string &tag, bool end) {
  if (tag.empty()) {
    return "";
  }

  if (end) {
    return "</" + tag + ">";
  }

  return "<" + tag + ">";
}

std::string parseTag(const std::string &tag, std::string &text, bool num) {
  std::string startTag = getTag(tag, false);
  std::string endTag = getTag(tag, true);

  size_t startPos = text.find(startTag);
  size_t endPos = text.find(endTag);
  size_t startTagL = startTag.length();

  if (startPos == std::string::npos || endPos == std::string::npos) {
    if (num) {
      return "0.0";
    }
    return "";
  }

  std::string sub =
      text.substr(startPos + startTagL, endPos - startPos - startTagL);

  return sub;
}

ObservationData extractObservation(std::string &xml) {
  ObservationData obs;
  obs.station_id = parseTag("station_id", xml, false);
  obs.location = parseTag("location", xml, false);
  obs.observation_time = parseTag("observation_time", xml, false);
  obs.observation_time_rfc822 = parseTag("observation_time_rfc822", xml, false);
  obs.weather = parseTag("weather", xml, false);
  obs.wind_dir = parseTag("wind_dir", xml, false);
  obs.latitude = parseTag("latitude", xml, true);
  obs.longitude = parseTag("longitude", xml, true);
  obs.temp_f = parseTag("temp_f", xml, true);
  obs.temp_c = parseTag("temp_c", xml, true);
  obs.relative_humidity = parseTag("relative_humidity", xml, true);
  obs.wind_degrees = parseTag("wind_degrees", xml, true);
  obs.wind_mph = parseTag("wind_mph", xml, true);
  obs.wind_gust_mph = parseTag("wind_gust_mph", xml, true);
  obs.wind_kt = parseTag("wind_kt", xml, true);
  obs.wind_gust_kt = parseTag("wind_gust_kt", xml, true);
  obs.pressure_mb = parseTag("pressure_mb", xml, true);
  obs.pressure_in = parseTag("pressure_in", xml, true);
  obs.dewpoint_f = parseTag("dewpoint_f", xml, true);
  obs.dewpoint_c = parseTag("dewpoint_c", xml, true);
  obs.visibility_mi = parseTag("visibility_mi", xml, true);

  return obs;
}

std::string formatXmlUrl(const std::string &station_abbrev) {
  return "https://forecast.weather.gov/xml/current_obs/" + station_abbrev +
         ".xml";
}

void printData(ObservationData &obs) {
  std::cout << "\n" << obs.abbrev << " | " << obs.observation_time << "\n";
  std::cout << "------------------" << "\n";
  std::cout << obs.weather << "\n";
  std::cout << obs.wind_mph << "\n";
  std::cout << obs.temp_f << "\n";
  std::cout << obs.dewpoint_f << "\n";
}

char *getCtime() {
  std::time_t currentTime = std::time(nullptr);
  return std::ctime(&currentTime);
}

void badRespHandler(cpr::Response &resp) {
  std::cout << resp.status_code << "\n";
  std::cout << resp.error.message << "\n";
  std::cout << "Waiting 1 hour, will try again...." << "\n";
  std::this_thread::sleep_for(std::chrono::hours(1));
}

int initSqlite(const char *sqlite_filename, sqlite3 *&db) {

  // initialize db
  if (sqlite3_open(sqlite_filename, &db) != SQLITE_OK) {
    std::cerr << "Error opening database: " << sqlite3_errmsg(db) << std::endl;
    return 1;
  }
  return 0;
}

int initTables(sqlite3 *db) {
  // 2. Create a Table
  const char *createObsTableSQL = "CREATE TABLE IF NOT EXISTS observations ("
                                  "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                                  "station_id TEXT NOT NULL,"
                                  "observation_time TEXT NOT NULL,"
                                  "observation_time_rfc822 TEXT NOT NULL,"
                                  "weather TEXT NOT NULL,"
                                  "wind_dir TEXT NOT NULL,"
                                  "temp_f FLOAT,"
                                  "temp_c FLOAT,"
                                  "relative_humidity FLOAT,"
                                  "wind_degrees FLOAT,"
                                  "wind_mph FLOAT,"
                                  "wind_gust_mph FLOAT,"
                                  "wind_kt FLOAT,"
                                  "wind_gust_kt FLOAT,"
                                  "pressure_mb FLOAT,"
                                  "pressure_in FLOAT,"
                                  "dewpoint_f FLOAT,"
                                  "dewpoint_c FLOAT,"
                                  "visibility_mi FLOAT,"
                                  "obshash TEXT NOT NULL UNIQUE);";

  const char *createStationTableSQL = "CREATE TABLE IF NOT EXISTS stations ("
                                      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                                      "location TEXT NOT NULL,"
                                      "station_id TEXT NOT NULL UNIQUE,"
                                      "abbrev TEXT NOT NULL UNIQUE,"
                                      "latitude FLOAT,"
                                      "longitude FLOAT);";

  if (sqlite3_exec(db, createObsTableSQL, nullptr, nullptr, nullptr) !=
      SQLITE_OK) {
    std::cerr << "Error creating table: " << sqlite3_errmsg(db) << std::endl;
    return 1;
  }

  if (sqlite3_exec(db, createStationTableSQL, nullptr, nullptr, nullptr) !=
      SQLITE_OK) {
    std::cerr << "Error creating table: " << sqlite3_errmsg(db) << std::endl;
    return 1;
  }

  return 0;
}

int stationInsert(sqlite3 *db, sqlite3_stmt *stmt, ObservationData &obs) {

  const char *insert_sql = "INSERT OR IGNORE INTO stations"
                           "(location, station_id, abbrev, latitude, longitude)"
                           "VALUES"
                           "(?, ?, ?, ?, ?)";

  if (sqlite3_prepare_v2(db, insert_sql, -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, obs.location.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, obs.station_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, obs.abbrev.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, obs.latitude.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, obs.longitude.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
      std::cerr << "Execution failed." << std::endl;
      return 1;
    }
    sqlite3_finalize(stmt);
    return 0;
  } else {
    return 1;
  }
}

int observationInsert(sqlite3 *db, sqlite3_stmt *stmt, ObservationData &obs) {
  const char *insert_sql =
      "INSERT OR IGNORE INTO observations"
      "(station_id,observation_time,observation_time_rfc822,weather,wind_dir,"
      "temp_f,temp_c,relative_humidity,wind_degrees,wind_mph,wind_gust_mph,"
      "wind_kt,wind_gust_kt,pressure_mb,pressure_in,dewpoint_f,dewpoint_c,"
      "visibility_mi, obshash)"
      "VALUES"
      "(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)";

  if (sqlite3_prepare_v2(db, insert_sql, -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, obs.station_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, obs.observation_time.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, obs.observation_time_rfc822.c_str(), -1,
                      SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, obs.weather.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, obs.wind_dir.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, obs.temp_f.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, obs.temp_c.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, obs.relative_humidity.c_str(), -1,
                      SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, obs.wind_degrees.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, obs.wind_mph.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, obs.wind_gust_mph.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, obs.wind_kt.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, obs.wind_gust_kt.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, obs.pressure_mb.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, obs.pressure_in.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, obs.dewpoint_f.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, obs.dewpoint_c.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, obs.visibility_mi.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, obs.visibility_mi.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, obs.obshash.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
      std::cerr << "Execution failed." << std::endl;
      return 1;
    }
    sqlite3_finalize(stmt);
    return 0;
  }
  return 1;
}

int main() {
  std::unordered_map<std::string, size_t> hashCache;
  std::hash<std::string> stringHasher;
  const char *sqlite_filename = "test.db";
  sqlite3 *db = nullptr;
  sqlite3_stmt *stmt;

  // initialize sqlite db and end program if theres an error.
  int initCode = initSqlite(sqlite_filename, db);
  if (initCode) {
    return initCode;
  }
  int initTableCode = initTables(db);
  if (initTableCode) {
    sqlite3_close(db);
    return initTableCode;
  }

  std::string abbrev = "KORD";
  std::string url = formatXmlUrl(abbrev);

  while (true) {
      cpr::Response resp = makeReq(url);
      if (resp.status_code != 200){
          badRespHandler(resp);
          continue;
      }

      std::string xml = resp.text;
      size_t hashedXml = stringHasher(xml);

      if (hashCache[abbrev] == hashedXml) {
          std::cout << "Observation Unchanged as of" << getCtime() << "\n";
          std::this_thread::sleep_for(std::chrono::hours(1));
          continue;
      }

      ObservationData obs = extractObservation(xml);
      obs.abbrev = abbrev;
      obs.obshash = hashedXml;

      printData(obs);


      int siCode = stationInsert(db, stmt, obs);
      if (siCode) {
        std::cout << abbrev << " | " << "STATION INSERT ERROR" << "\n";
      }
    
      int oiCode = observationInsert(db, stmt, obs);
      if (oiCode) {
        std::cout << abbrev << " | " <<"OBSERVATION INSERT ERROR" << "\n";
      }
      std::this_thread::sleep_for(std::chrono::hours(1));


  }

  sqlite3_close(db);
  return 0;
}