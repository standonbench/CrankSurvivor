#include "game.h"

// ---------------------------------------------------------------------------
// Spatial hash grid for enemy collision queries
// ---------------------------------------------------------------------------
typedef struct {
    uint8_t count;
    uint8_t indices[COL_MAX_PER_CELL];
} GridCell;

static GridCell grid[COL_ROWS][COL_COLS];

void collision_clear(void)
{
    for (int r = 0; r < COL_ROWS; r++)
        for (int c = 0; c < COL_COLS; c++)
            grid[r][c].count = 0;
}

void collision_insert_enemy(int idx, float x, float y)
{
    int col = (int)(x / COL_CELL_SIZE);
    int row = (int)(y / COL_CELL_SIZE);
    col = clampi(col, 0, COL_COLS - 1);
    row = clampi(row, 0, COL_ROWS - 1);

    GridCell* cell = &grid[row][col];
    if (cell->count < COL_MAX_PER_CELL) {
        cell->indices[cell->count++] = (uint8_t)idx;
    }
}

// Query enemies within radius of (x,y), return indices in outIndices
int collision_query_point(float x, float y, float radius,
                          int* outIndices, int maxResults)
{
    int col = (int)(x / COL_CELL_SIZE);
    int row = (int)(y / COL_CELL_SIZE);
    float r2 = radius * radius;
    int hits = 0;

    for (int dr = -1; dr <= 1; dr++) {
        for (int dc = -1; dc <= 1; dc++) {
            int r = row + dr, c = col + dc;
            if (r < 0 || r >= COL_ROWS || c < 0 || c >= COL_COLS) continue;

            GridCell* cell = &grid[r][c];
            for (int i = 0; i < cell->count && hits < maxResults; i++) {
                int ei = cell->indices[i];
                if (ei < enemyCount && enemies[ei].alive) {
                    float d2 = dist_sq(x, y, enemies[ei].x, enemies[ei].y);
                    if (d2 < r2) {
                        outIndices[hits++] = ei;
                    }
                }
            }
        }
    }
    return hits;
}
