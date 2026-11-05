#!/usr/bin/env python3
"""The three ballrooms of MASCARADE, drawn with a few helpers and written to venues/*.json."""
import json, os
OUT = os.path.join(os.path.dirname(__file__), '..', 'venues')

class Map:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.g = [['.' for _ in range(w)] for _ in range(h)]
        for x in range(w):
            self.g[0][x] = self.g[h - 1][x] = '#'
        for y in range(h):
            self.g[y][0] = self.g[y][w - 1] = '#'
    def put(self, x, y, c):
        self.g[y][x] = c
    def rect(self, x0, y0, x1, y1, c):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                self.g[y][x] = c
    def rows(self):
        return [''.join(r) for r in self.g]

def salon():
    m = Map(36, 22)
    # Dance floor in the middle, the orchestra's corner, columns around.
    m.rect(11, 7, 24, 14, ',')
    m.put(17, 10, 'D'); m.put(18, 11, 'D')
    for x in (9, 26):
        for y in (5, 9, 13, 16):
            m.put(x, y, 'O')
    # Buffet along the north wall (two tables) with standing spots.
    m.rect(5, 2, 10, 2, 'T'); m.rect(25, 2, 30, 2, 'T')
    for x in (5, 7, 9): m.put(x, 3, 'b')
    for x in (26, 28, 30): m.put(x, 3, 'b')
    m.put(27, 3, 'x')
    # Paintings on the north and side walls (the south side is the gallery).
    for x in (3, 12, 23, 32): m.put(x, 1, 'p')
    for y in (6, 12): m.put(1, y, 'p'); m.put(34, y, 'p')
    # Statues and their admirers.
    m.put(4, 9, 'S'); m.put(5, 9, 's'); m.put(31, 9, 'S'); m.put(30, 9, 'u'); m.put(31, 15, 'S'); m.put(30, 15, 's')
    # Conversation circles.
    for (x, y) in ((5, 14), (14, 4), (21, 4), (17, 17), (29, 18), (6, 18)):
        m.put(x, y, 'c')
    # Writing desk, sofas, windows, entrances.
    m.put(33, 18, 'W'); m.put(32, 18, 'l')
    m.rect(2, 16, 2, 17, 'H'); m.put(3, 16, 'h'); m.put(3, 17, 'h')
    m.put(17, 1, 'n'); m.put(18, 1, 'n')
    m.put(1, 2, 'e'); m.put(34, 2, 'e'); m.put(17, 20, 'e')
    return m

def serre():
    m = Map(38, 24)
    # A central fountain, palm beds, gravel paths, a small dance pavilion.
    m.rect(17, 10, 20, 13, 'F')
    for (x, y) in ((16, 11), (21, 12), (18, 9), (19, 14)): m.put(x, y, 's')
    for (x0, y0) in ((5, 4), (30, 4), (5, 17), (30, 17)):
        m.rect(x0, y0, x0 + 2, y0 + 2, 'O')
    m.rect(8, 9, 12, 14, ','); m.put(10, 11, 'D')
    m.rect(26, 9, 30, 14, ','); m.put(28, 12, 'D')
    m.rect(14, 2, 23, 2, 'T')
    for x in (14, 16, 18, 20, 22): m.put(x, 3, 'b')
    m.put(19, 3, 'x')
    for (x, y) in ((4, 9), (34, 9), (4, 15), (34, 15)): m.put(x, y, 'S')
    m.put(5, 9, 's'); m.put(33, 9, 'u'); m.put(5, 15, 's'); m.put(33, 15, 's')
    for (x, y) in ((10, 3), (27, 3), (10, 20), (27, 20), (18, 18), (14, 7), (23, 7)): m.put(x, y, 'c')
    for x in (3, 9, 28, 34): m.put(x, 1, 'n')
    m.put(36, 20, 'W'); m.put(35, 20, 'l')
    m.rect(1, 11, 1, 12, 'H'); m.put(2, 11, 'h'); m.put(2, 12, 'h')
    for y in (5, 18): m.put(1, y, 'p'); m.put(36, y, 'p')
    m.put(1, 2, 'e'); m.put(36, 2, 'e'); m.put(18, 22, 'e')
    return m

def opera():
    m = Map(40, 22)
    # The grand staircase (blocked), mirrors on the walls, loges along the sides, a foyer to dance in.
    m.rect(17, 1, 22, 5, 'T')
    for x in (15, 24): m.put(x, 4, 'S')
    m.put(16, 4, 's'); m.put(23, 4, 'u')
    m.rect(14, 10, 25, 16, ','); m.put(19, 13, 'D'); m.put(20, 13, 'D')
    for x in (6, 12, 27, 33):
        for y in (7, 14): m.put(x, y, 'O')
    m.rect(3, 19, 9, 19, 'T'); m.rect(30, 19, 36, 19, 'T')
    for x in (3, 5, 7, 9): m.put(x, 18, 'b')
    for x in (31, 33, 35): m.put(x, 18, 'b')
    m.put(5, 18, 'x')
    for x in (3, 9, 30, 36): m.put(x, 1, 'p')
    for y in (5, 10, 15): m.put(1, y, 'p'); m.put(38, y, 'p')
    for (x, y) in ((8, 4), (31, 4), (9, 11), (30, 11), (19, 8), (14, 18), (25, 18)): m.put(x, y, 'c')
    m.put(38, 18, 'W'); m.put(37, 18, 'l')
    m.rect(1, 12, 1, 13, 'H'); m.put(2, 12, 'h'); m.put(2, 13, 'h')
    m.put(12, 1, 'n'); m.put(27, 1, 'n')
    m.put(1, 2, 'e'); m.put(38, 2, 'e'); m.put(20, 20, 'e')
    return m

def main():
    os.makedirs(OUT, exist_ok=True)
    for vid, fn, n in (('salon', salon, 90), ('serre', serre, 100), ('opera', opera, 95)):
        m = fn()
        with open(os.path.join(OUT, f'{vid}.json'), 'w') as f:
            json.dump({'id': vid, 'rows': m.rows(), 'guests': n}, f)
        print(vid)
        print('\n'.join(m.rows()))

if __name__ == '__main__':
    main()
