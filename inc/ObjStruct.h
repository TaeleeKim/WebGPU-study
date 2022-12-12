#pragma once
#ifndef _OBJSTRUCT_H_
#define _OBJSTRUCT_H_

// Vector - STD Vector/Array Library
#include <vector>

struct Vector2
{
	// Default Constructor
	Vector2()
	{
		X = 0.0f;
		Y = 0.0f;
	}
	// Variable Set Constructor
	Vector2(float X_, float Y_)
	{
		X = X_;
		Y = Y_;
	}
	// Bool Equals Operator Overload
	bool operator==(const Vector2& other) const
	{
		return (this->X == other.X && this->Y == other.Y);
	}
	// Bool Not Equals Operator Overload
	bool operator!=(const Vector2& other) const
	{
		return !(this->X == other.X && this->Y == other.Y);
	}
	// Addition Operator Overload
	Vector2 operator+(const Vector2& right) const
	{
		return Vector2(this->X + right.X, this->Y + right.Y);
	}
	// Subtraction Operator Overload
	Vector2 operator-(const Vector2& right) const
	{
		return Vector2(this->X - right.X, this->Y - right.Y);
	}
	// Float Multiplication Operator Overload
	Vector2 operator*(const float& other) const
	{
		return Vector2(this->X *other, this->Y * other);
	}

	// Positional Variables
	float X;
	float Y;
};

// Structure: Vector3
//
// Description: A 3D Vector that Holds Positional Data
struct Vector3
{
	// Default Constructor
	Vector3()
	{
		X = 0.0f;
		Y = 0.0f;
		Z = 0.0f;
	}
	// Variable Set Constructor
	Vector3(float X_, float Y_, float Z_)
	{
		X = X_;
		Y = Y_;
		Z = Z_;
	}
	// Bool Equals Operator Overload
	bool operator==(const Vector3& other) const
	{
		return (this->X == other.X && this->Y == other.Y && this->Z == other.Z);
	}
	// Bool Not Equals Operator Overload
	bool operator!=(const Vector3& other) const
	{
		return !(this->X == other.X && this->Y == other.Y && this->Z == other.Z);
	}
	// Addition Operator Overload
	Vector3 operator+(const Vector3& right) const
	{
		return Vector3(this->X + right.X, this->Y + right.Y, this->Z + right.Z);
	}
	// Subtraction Operator Overload
	Vector3 operator-(const Vector3& right) const
	{
		return Vector3(this->X - right.X, this->Y - right.Y, this->Z - right.Z);
	}
	// Float Multiplication Operator Overload
	Vector3 operator*(const float& other) const
	{
		return Vector3(this->X * other, this->Y * other, this->Z * other);
	}
	// Float Division Operator Overload
	Vector3 operator/(const float& other) const
	{
		return Vector3(this->X / other, this->Y / other, this->Z / other);
	}

	// Positional Variables
	float X;
	float Y;
	float Z;
};

// Structure: Vertex
//
// Description: Model Vertex object that holds
//	a Position, Normal, and Texture Coordinate
struct Vertex
{
	// Position Vector
	Vector3 Position;

	// Normal Vector
	Vector3 Normal;

	// Texture Coordinate Vector
	Vector2 TextureCoordinate;
};

struct Material
{
	Material()
	{
		name;
		Ns = 0.0f;
		Ni = 0.0f;
		d = 0.0f;
		illum = 0;
	}

	// Material Name
	std::string name;
	// Ambient Color
	Vector3 Ka;
	// Diffuse Color
	Vector3 Kd;
	// Specular Color
	Vector3 Ks;
	// Specular Exponent
	float Ns;
	// Optical Density
	float Ni;
	// Dissolve
	float d;
	// Illumination
	int illum;
	// Ambient Texture Map
	std::string map_Ka;
	// Diffuse Texture Map
	std::string map_Kd;
	// Specular Texture Map
	std::string map_Ks;
	// Specular Hightlight Map
	std::string map_Ns;
	// Alpha Texture Map
	std::string map_d;
	// Bump Map
	std::string map_bump;
};

// Structure: Mesh
//
// Description: A Simple Mesh Object that holds
//	a name, a vertex list, and an index list
struct Mesh
{
	// Default Constructor
	Mesh()
	{

	}
	// Variable Set Constructor
	Mesh(std::vector<Vertex>& _Vertices, std::vector<unsigned int>& _Indices)
	{
		Vertices = _Vertices;
		Indices = _Indices;
	}
	// Mesh Name
	std::string MeshName;
	// Vertex List
	std::vector<Vertex> Vertices;
	// Index List
	std::vector<unsigned int> Indices;

	// Material
	Material MeshMaterial;
};






#endif /* _OBJSTRUCT_H_ */
