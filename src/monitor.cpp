#include <iostream>
#include <sqlite3.h>


int selectObsData(sqlite3* db){
    const char* select_sql = "SELECT id, station_id, temp_f, observation_time  FROM observations;";    

    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, select_sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return 1;
    }

    // 3. Step through the results row by row
    while (sqlite3_step(stmt) == SQLITE_ROW) {

        // Extract data based on column index (starting at 0)
        int id = sqlite3_column_int(stmt, 0);
        const char* station_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        float tempF = sqlite3_column_double(stmt, 2);
        const char* observation_time = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));


        std::cout << "ID: " << id << " | StationID: " << station_id << " | Temperature (F): " << tempF << " | Time: " << observation_time << std::endl;
    }


    sqlite3_finalize(stmt);
    return 0;
}


int initSqlite(const char *sqlite_filename, sqlite3 *&db) {

    // initialize db
    if (sqlite3_open(sqlite_filename, &db) != SQLITE_OK) {
      std::cerr << "Error opening database: " << sqlite3_errmsg(db) << std::endl;
      return 1;
    }
    return 0;
  }


int main(){
    sqlite3* db = nullptr;
    const char* sqlite_filename =  "test.db";
    int initCode = initSqlite(sqlite_filename, db);

    int sCode = selectObsData(db);

    sqlite3_close(db);

}