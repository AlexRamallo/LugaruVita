#pragma endian little

struct PVRHeader {
	u32 Version;
	u32 Flags;
	u64 PixelFormat;
	u32 ColorSpace;
	u32 ChannelType;
	u32 Height;
	u32 Width;
	u32 Depth;
	u32 NumSurfaces;
	u32 NumFaces;
	u32 MipMapCount;
	u32 MetaDataSize;
};

struct NormalMapInfo {
	u32 Scale;
	u8 Order[4];
};

struct CubeMapInfo {
	u8 Order[6];
};

struct TextureOrientationInfo {
	u8 X;
	u8 Y;
	u8 Z;
};

struct BorderInfo {
	u32 X;
	u32 Y;
	u32 Z;
};

struct PVRMetaData {
	u8 FourCC[4];
	u32 Key;
	u32 DataSize;
	
	if(FourCC[0] == 'P' && FourCC[1] == 'V' && FourCC[2] == 'R'){
		if(FourCC[3] == 3){
			if(Key == 0){
				/*
					An array of integers describing the position and sizes of each
					texture within a texture atlas. Each sequence of four integers
					represents the information for a single texture within the atlas and
					appear in the order:
					
					1. X Position
					2. Y Position
					3. Width
					4. Height
				*/
				u8 Data[DataSize];
			}else if(Key == 1){
				/*
					Specifies that the file contains normal map information. The 8
					bytes are in the form of a 32bit float representing the scale of the
					normal map, followed by a four character array describing the
					order of the channels, for example {‘x’, ‘y’, ‘z’, ‘h’}. Use of ‘h’ as the
					representation for a given channel denotes that the channel in
					question contains the original height map.
				*/
				NormalMapInfo Data;
			}else if(Key == 2){
				/*
					Specifies that the file contains a cube map and the order of the
					faces within that cube map. The 6 bytes represent a six character
					string. This string shows the order the cube map faces are stored
					in the texture data, for example ‘XxYyZz’. Uppercase letters refer
					to a positive axis position while lowercase refer to a negative axis
					position. Not all axes must be present.		
				*/
				CubeMapInfo Data;
			}else if(Key == 3){
				/*
					Specifies the logical orientation of the texture within the texture
					data. This does not affect the mapping from pixels to texture
					coordinates. Each byte is a Boolean value representing the
					orientation for a single axis in the order X, Y, Z. The values are as
					follows:
						X Axis
							Non-zero value = X values increase to the left
							Zero value = X values increase to the right
						Y Axis
							Non-zero value = Y values increase upwards
							Zero value = Y values increase downwards
						Z Axis
							Non-zero value = Z values increase outwards
							Zero value = Z values increase inwards
				*/
				TextureOrientationInfo Data;
			}else if(Key == 4){
				/*
					Specifies whether the texture has a border. The 12 bytes are
					broken down into three unsigned 32bit integers. The three
					integers represent the size of the border of the image, in pixels, on
					the X, Y and Z axes, respectively. These values are used to offset
					texture reads by the size of the border in order to obtain the actual
					texture data.

					It should be noted that only three border sizes are given, this
					means that the border size for X is applied to both the left and
					right of the image, Y to the top and bottom and Z to the front and
					back.
				*/
				BorderInfo Data;
			}else if(Key == 5){
				u8 Padding[DataSize];
			}else if(Key == 6){
				/*
				    Contains overrides for 'ChannelType' specified in header
				    Not sure exactly how...
			    */
				u8 Data[DataSize]; 
			}else if(Key == 7){
			    /*
			    	Support data for Binomial ETC1S and UASTC super-compression encoding schemes
			    */
			    u8 Data[DataSize];
			}else if(Key == 8){
			    /*
			        MAX_RANGE value used when encoding RGBD/RGBM texture formats.
			        This value is needed by the decoder
			    */
			    float Data;
			}
		}else break;
	}else break;
};

struct PVR {
	PVRHeader Header;
	PVRMetaData MetaData[1024];
	u8 TextureData[];
};

PVR pvr @ 0x00;



