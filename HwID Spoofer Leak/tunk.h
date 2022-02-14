#pragma once

#define tunk() UseFuntion(667, 7854, 3624, 8951,120.6f);

inline int UseFuntion(int x, int y, int z, int r, float ks)
{
	ks /= 2;
	for (int i = 2; i < 8; i++)
	{
		z += 456;
		r -= 55;
		x++;
		y--;

	}
	for (int i = 0; i < 2; i++)
	{
		x++;
		y += 234;
		z -= 23;
		r += 634;
	}
	return x + z + r + y;
}