#include "MazeData.h"
#include <cstdlib>
#include <utility>

MazeData::MazeData(int size) : n(size), grid(size * size, '#') {
    // start at (0,0)
    carve(0, 0);  

    // even size: connect the corner
    if (n % 2 == 0) {
        grid[(n - 2) * n + n - 1] = '.'; 
        steps.push_back({n - 2, n - 1});
    }

    // exit, bottom-right
    grid[(n - 1) * n + n - 1] = 'E';  
    steps.push_back({n - 1, n - 1});
}

void MazeData::carve(int r, int c) {
    grid[r * n + c] = '.';
    steps.push_back({r, c});
    int dr[4] = {-2, 2, 0, 0};
    int dc[4] = {0, 0, -2, 2};
    int o[4] = {0, 1, 2, 3};
    for (int i = 3; i > 0; i--) std::swap(o[i], o[std::rand() % (i + 1)]);

    for (int k = 0; k < 4; k++) {
        int nr = r + dr[o[k]], nc = c + dc[o[k]];
        if (nr >= 0 && nr < n && nc >= 0 && nc < n &&
            grid[nr * n + nc] == '#') {
            int wallR = r + dr[o[k]] / 2;
            int wallC = c + dc[o[k]] / 2;
            grid[wallR * n + wallC] = '.';
            steps.push_back({wallR, wallC});
            carve(nr, nc);
        }
    }
}

int MazeData::size() const { return n; }
const std::vector<MazeStep>& MazeData::generationSteps() const { return steps; }

char MazeData::cell(int r, int c) const {
    if (r < 0 || r >= n || c < 0 || c >= n) return '#';
    return grid[r * n + c];
}