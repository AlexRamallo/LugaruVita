#pragma endian big

struct Vertex {
	float x;
	float y;
	float z;
};

struct Triangle {
	u16 vertIndex0;
	u16 unused;

	u16 vertIndex1;
	u16 unused;
	
	u16 vertIndex2;
	u16 unused;
	
	float texcoord_x[3];
	float texcoord_y[3];
};

struct Model {
	u16 vertexNum;
	u16 triangleNum;
	
	Vertex vertices[vertexNum];
	Triangle triangles[triangleNum];
};

Model model @ 0x00;
