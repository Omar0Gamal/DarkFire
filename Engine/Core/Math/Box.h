#pragma once

namespace Core
{
	class Box
	{
	public:
		Box() {}
		Box(unsigned int width, unsigned int height) : width(width), height(height) {}
		Box(unsigned int left, unsigned int top, unsigned int width, unsigned int height) :left(left), top(top), width(width), height(height) {}
		Box(const Box& other) :left(other.left), top(other.top), width(other.width), height(other.height) {}
	public:
		unsigned int width = 0, height = 0, left = 0, top = 0;
	};
}