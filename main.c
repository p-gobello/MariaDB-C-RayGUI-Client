#include <stdio.h>
#include <string.h>
#include "include/raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "include/raygui.h"
#include "include/mysql.h"

#define MAX_DATABASES 100 // Increased the limit to handle more databases
#define MAX_TABLES 500    // Increased the limit to handle more tables
#define MAX_TABLE_NAME_LEN 150

// Structure to hold application state
typedef struct {
    int currentScreen;
    char username[100];
    char password[100];
    char port[10];
    char selectedDatabase[100];
    char selectedTable[100]; // Added to store the selected table
    char tables[MAX_TABLES][MAX_TABLE_NAME_LEN];
    int numTables;
    int scrollOffset;
    int numDatabases;
    char databases[MAX_DATABASES][100];
    bool usernameEditMode;
    bool passwordEditMode;
    bool portEditMode;
    bool showTableData;
    char tableData[1024]; // Assuming data fits in this buffer
} AppState;

enum Screens {
    SCREEN_LOGIN,
    SCREEN_DATABASES,
    SCREEN_TABLES,
    SCREEN_TABLE_DATA // Added screen for displaying table data
};

// Function to fetch tables for a given database
void fetchTables(AppState *state, MYSQL *connection) {
    MYSQL_RES *result;
    MYSQL_ROW row;
    state->numTables = 0;

    char query[256];
    snprintf(query, sizeof(query), "SHOW TABLES FROM %s", state->selectedDatabase);

    if (mysql_query(connection, query)) {
        printf("Error querying tables: %s\n", mysql_error(connection));
        return;
    }

    result = mysql_store_result(connection);
    if (result == NULL) {
        printf("Error storing result: %s\n", mysql_error(connection));
        return;
    }

    while ((row = mysql_fetch_row(result))) {
        if (state->numTables < MAX_TABLES) {
            strncpy(state->tables[state->numTables], row[0], MAX_TABLE_NAME_LEN - 1);
            state->tables[state->numTables][MAX_TABLE_NAME_LEN - 1] = '\0'; // Ensure null-termination
            state->numTables++;
        } else {
            printf("Warning: Maximum number of tables reached.\n");
            break;
        }
    }

    mysql_free_result(result);
}

// Function to fetch data from a selected table
void fetchTableData(AppState *state, MYSQL *connection, const char *tableName) {
    // Check if a database is selected
    if (strlen(state->selectedDatabase) == 0) {
        printf("Error: No database selected.\n");
        return;
    }

    // Construct query to fetch all data from the table
    char query[256];
    snprintf(query, sizeof(query), "SELECT * FROM %s.%s", state->selectedDatabase, tableName);

    // Print the query for debugging
    printf("Query: %s\n", query);

    // Ensure database connection is established
    if (mysql_ping(connection) != 0) {
        printf("Error: Database connection lost.\n");
        return;
    }

    // Execute the query
    if (mysql_query(connection, query)) {
        printf("Error executing query: %s\n", mysql_error(connection));
        return;
    }

    // Store the result
    MYSQL_RES *result = mysql_store_result(connection);
    if (result == NULL) {
        printf("Error storing result: %s\n", mysql_error(connection));
        return;
    }

    // Prepare buffer for table data
    strcpy(state->tableData, "");

    // Build string with table data
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        for (int i = 0; i < mysql_num_fields(result); i++) {
            strcat(state->tableData, row[i] ? row[i] : "NULL");
            strcat(state->tableData, " ");
        }
        strcat(state->tableData, "\n");
    }

    // Print the fetched data for debugging
    printf("Fetched data:\n%s\n", state->tableData);

    // Free the result
    mysql_free_result(result);

    // Set flag to show table data
    state->showTableData = true;
}

int main() {
    // Initialize raylib window
    const int screenWidth = 800;
    const int screenHeight = 600;

    InitWindow(screenWidth, screenHeight, "MariaDB GUI Client");

    AppState state = {0};
    state.currentScreen = SCREEN_LOGIN;
    state.usernameEditMode = true;

    SetTargetFPS(60); // Set target frames per second

    // Initialize MySQL connection (but don't connect yet)
    MYSQL *connection = mysql_init(NULL);
    if (connection == NULL) {
        printf("mysql_init() failed\n");
        return 1;
    }

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        if (state.currentScreen == SCREEN_LOGIN) {
            // Login screen
            DrawText("MariaDB Login", 350, 50, 20, DARKGRAY);
            DrawText("Username:", 200, 150, 20, DARKGRAY);
            DrawText("Password:", 200, 200, 20, DARKGRAY);
            DrawText("Port:", 200, 250, 20, DARKGRAY);

            // Handle input field focus
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                Vector2 mousePos = GetMousePosition();
                if (CheckCollisionPointRec(mousePos, (Rectangle){300, 150, 200, 30})) {
                    state.usernameEditMode = true;
                    state.passwordEditMode = false;
                    state.portEditMode = false;
                } else if (CheckCollisionPointRec(mousePos, (Rectangle){300, 200, 200, 30})) {
                    state.usernameEditMode = false;
                    state.passwordEditMode = true;
                    state.portEditMode = false;
                } else if (CheckCollisionPointRec(mousePos, (Rectangle){300, 250, 200, 30})) {
                    state.usernameEditMode = false;
                    state.passwordEditMode = false;
                    state.portEditMode = true;
                } else {
                    state.usernameEditMode = false;
                    state.passwordEditMode = false;
                    state.portEditMode = false;
                }
            }

            // Update text fields
            if (state.usernameEditMode) {
                GuiTextBox((Rectangle){300, 150, 200, 30}, state.username, sizeof(state.username), true);
            } else {
                GuiTextBox((Rectangle){300, 150, 200, 30}, state.username, sizeof(state.username), false);
            }

            if (state.passwordEditMode) {
               GuiTextBox((Rectangle){300, 200, 200, 30}, state.password, sizeof(state.password), true);
            } else {
                GuiTextBox((Rectangle){300, 200, 200, 30}, state.password, sizeof(state.password), false);
            }

            if (state.portEditMode) {
                GuiTextBox((Rectangle){300, 250, 200, 30}, state.port, sizeof(state.port), true);
            } else {
                GuiTextBox((Rectangle){300, 250, 200, 30}, state.port, sizeof(state.port), false);
            }

            if (GuiButton((Rectangle){350, 300, 100, 40}, "Connect")) {
                // Convert port to integer
                int port = atoi(state.port);

                // Connect to MariaDB
                if (!mysql_real_connect(connection, "localhost", state.username, state.password, NULL, port, NULL, 0)) {
                    printf("Failed to connect to MariaDB: %s\n", mysql_error(connection));
                } else {
                    // Fetch databases
                    MYSQL_RES *result;
                    MYSQL_ROW row;
                    state.numDatabases = 0;

                    if (mysql_query(connection, "SHOW DATABASES")) {
                        printf("Error querying databases: %s\n", mysql_error(connection));
                    } else {
                        result = mysql_store_result(connection);
                        if (result == NULL) {
                            printf("Error storing result: %s\n", mysql_error(connection));
                        } else {
                            while ((row = mysql_fetch_row(result))) {
                                if (state.numDatabases < MAX_DATABASES) {
                                    strncpy(state.databases[state.numDatabases], row[0], sizeof(state.databases[state.numDatabases]) - 1);
                                    state.databases[state.numDatabases][sizeof(state.databases[state.numDatabases]) - 1] = '\0'; // Ensure null-termination
                                    state.numDatabases++;
                                } else {
                                    printf("Warning: Maximum number of databases reached.\n");
                                    break;
                                }
                            }

                            mysql_free_result(result);
                            state.currentScreen = SCREEN_DATABASES;
                        }
                    }
                }
            }
        } else if (state.currentScreen == SCREEN_DATABASES) {
            // Update scroll offset based on mouse wheel movement
            int maxScroll = (state.numDatabases - 1) * 50 - (screenHeight - 200);
            if (maxScroll < 0) maxScroll = 0;
            state.scrollOffset += GetMouseWheelMove() * 20; // Adjust scroll speed as needed
            if (state.scrollOffset > 0) state.scrollOffset = 0;
            if (state.scrollOffset < -maxScroll) state.scrollOffset = -maxScroll;

            // Display database buttons with scroll
            int startY = 100 + state.scrollOffset;

            for (int i = 0; i < state.numDatabases; i++) {
                if (GuiButton((Rectangle){100, startY + 50 * i, 200, 40}, state.databases[i])) {
                    // Button clicked, fetch tables for the selected database
                    strncpy(state.selectedDatabase, state.databases[i], sizeof(state.selectedDatabase) - 1);
                    state.selectedDatabase[sizeof(state.selectedDatabase) - 1] = '\0'; // Ensure null-termination
                    fetchTables(&state, connection);
                    state.currentScreen = SCREEN_TABLES;
                    state.scrollOffset = 0; // Reset scroll offset for tables screen
                }
            }
        } else if (state.currentScreen == SCREEN_TABLES) {
            // Update scroll offset based on mouse wheel movement
            int maxScroll = (state.numTables - 1) * 30 - (screenHeight - 200);
            if (maxScroll < 0) maxScroll = 0;
            state.scrollOffset += GetMouseWheelMove() * 20; // Adjust scroll speed as needed
            if (state.scrollOffset > 0) state.scrollOffset = 0;
            if (state.scrollOffset < -maxScroll) state.scrollOffset = -maxScroll;

            // Display tables of the selected database with scroll
            DrawText(TextFormat("Tables in database: %s", state.selectedDatabase), 100, 50, 20, DARKGRAY);

            int startY = 100 + state.scrollOffset;

            for (int i = 0; i < state.numTables; i++) {
                if (GuiButton((Rectangle){100, startY + 30 * i, 200, 30}, state.tables[i])) {
                    // Button clicked, fetch data for the selected table
                    strncpy(state.selectedTable, state.tables[i], sizeof(state.selectedTable) - 1);
                    state.selectedTable[sizeof(state.selectedTable) - 1] = '\0'; // Ensure null-termination
                    fetchTableData(&state, connection, state.tables[i]);
                    state.currentScreen = SCREEN_TABLE_DATA;
                }
            }

            if (GuiButton((Rectangle){600, 500, 150, 40}, "Back")) {
                state.currentScreen = SCREEN_DATABASES;
                state.scrollOffset = 0; // Reset scroll offset for databases screen
            }
        } else if (state.currentScreen == SCREEN_TABLE_DATA) {
            // Display table data
            DrawText(state.tableData, 350, 100, 20, DARKGRAY);

            if (GuiButton((Rectangle){600, 500, 150, 40}, "Back")) {
                state.currentScreen = SCREEN_TABLES;
            }
        }

        EndDrawing();
    }

    // Close MySQL connection before exiting
    mysql_close(connection);
    CloseWindow();
    return 0;
}
