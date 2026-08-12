/*
 *  Quackle -- Crossword game artificial intelligence and analysis tool
 *  Copyright (C) 2005-2019 Jason Katz-Brown, John O'Laughlin, and John Fultz.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "crossplayboards.h"

// Board layout sourced from kamilmielnik/scrabble-solver crossplay.ts config.
// Coordinates in that config use (x=col, y=row); arrays below use [row][col].
//
// Premium square counts: 20 TL, 20 DL, 8 TW, 8 DW = 56 total.
// Center square (7,7) has NO bonus (unlike Scrabble's DWS).

CrossplayBoard::CrossplayBoard()
{
	m_name = MARK_UV("Crossplay Board");

	// Letter multipliers: 1=normal, 2=double letter, 3=triple letter
	const int letterm[15][15] = {
		//  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14
		{  3, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 3 }, // row 0
		{  1, 1, 1, 1, 1, 1, 3, 1, 3, 1, 1, 1, 1, 1, 1 }, // row 1
		{  1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1 }, // row 2
		{  1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1 }, // row 3
		{  1, 1, 2, 1, 1, 3, 1, 1, 1, 3, 1, 1, 2, 1, 1 }, // row 4
		{  1, 1, 1, 1, 3, 1, 1, 2, 1, 1, 3, 1, 1, 1, 1 }, // row 5
		{  1, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 1 }, // row 6
		{  2, 1, 1, 1, 1, 2, 1, 1, 1, 2, 1, 1, 1, 1, 2 }, // row 7
		{  1, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 1 }, // row 8
		{  1, 1, 1, 1, 3, 1, 1, 2, 1, 1, 3, 1, 1, 1, 1 }, // row 9
		{  1, 1, 2, 1, 1, 3, 1, 1, 1, 3, 1, 1, 2, 1, 1 }, // row 10
		{  1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1 }, // row 11
		{  1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1 }, // row 12
		{  1, 1, 1, 1, 1, 1, 3, 1, 3, 1, 1, 1, 1, 1, 1 }, // row 13
		{  3, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 3 }, // row 14
	};

	// Word multipliers: 1=normal, 2=double word, 3=triple word
	const int wordm[15][15] = {
		//  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14
		{  1, 1, 1, 3, 1, 1, 1, 1, 1, 1, 1, 3, 1, 1, 1 }, // row 0
		{  1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1 }, // row 1
		{  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, // row 2
		{  3, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 3 }, // row 3
		{  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, // row 4
		{  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, // row 5
		{  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, // row 6
		{  1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1 }, // row 7
		{  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, // row 8
		{  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, // row 9
		{  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, // row 10
		{  3, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 3 }, // row 11
		{  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, // row 12
		{  1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1 }, // row 13
		{  1, 1, 1, 3, 1, 1, 1, 1, 1, 1, 1, 3, 1, 1, 1 }, // row 14
	};

	for (int i = 0; i < 15; ++i)
	{
		for (int j = 0; j < 15; ++j)
		{
			m_letterMultipliers[i][j] = letterm[i][j];
			m_wordMultipliers[i][j] = wordm[i][j];
		}
	}
}
