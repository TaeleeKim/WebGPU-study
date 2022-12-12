#pragma once
#ifndef _OBJFUNCTION_H_
#define _OBJFUNCTION_H_

#include "ObjStruct.h"
// Namespace: Math
//
// Description: The namespace that holds all of the math
//	functions need for OBJL
namespace objMath
{
	// Vector3 Cross Product
	Vector3 CrossV3(const Vector3 a, const Vector3 b);


	// Vector3 Magnitude Calculation
	float MagnitudeV3(const Vector3 in);


	// Vector3 DotProduct
	float DotV3(const Vector3 a, const Vector3 b);


	// Angle between 2 Vector3 Objects
	float AngleBetweenV3(const Vector3 a, const Vector3 b);

	// Projection Calculation of a onto b
	Vector3 ProjV3(const Vector3 a, const Vector3 b);
}
// Namespace: Algorithm
//
// Description: The namespace that holds all of the
// Algorithms needed for OBJL
namespace objAlgorithm
{
	// Vector3 Multiplication Opertor Overload
	Vector3 operator*(const float& left, const Vector3& right);


	// A test to see if P1 is on the same side as P2 of a line segment ab
	bool SameSide(Vector3 p1, Vector3 p2, Vector3 a, Vector3 b);


	// Generate a cross produect normal for a triangle
	Vector3 GenTriNormal(Vector3 t1, Vector3 t2, Vector3 t3);

	// Check to see if a Vector3 Point is within a 3 Vector3 Triangle
	bool inTriangle(Vector3 point, Vector3 tri1, Vector3 tri2, Vector3 tri3);
	

	// Split a String into a string array at a given token
	void split(const std::string &in,
		std::vector<std::string> &out,
		std::string token);


	// Get tail of string after first token and possibly following spaces
	std::string tail(const std::string &in);


	// Get first token of string
	std::string firstToken(const std::string &in);


	// Get element at given index position
	template <class T>
	const T & getElement(const std::vector<T> &elements, std::string &index)
	{
		int idx = std::stoi(index);
		if (idx < 0)
			idx = int(elements.size()) + idx;
		else
			idx--;
		return elements[idx];
	}

}

#endif /* _OBJFUNCTION_H_ */
