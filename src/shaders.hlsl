// resize & convert input
Texture2D<float3> input : register(t0);

// resize output, packed BGRA format
RWTexture2D<uint> output : register(u0);

// convert output
RWTexture2D<uint>  outputY  : register(u0);
RWTexture2D<uint2> outputUV : register(u1);

// YUV conversion matrix
cbuffer ConvertBuffer : register(b0) {
	row_major float3x4 convertMtx;
};

float Filter(float x) {
	// https://en.wikipedia.org/wiki/Mitchell%E2%80%93Netravali_filters
	// with B=C=1/3
	
	x = abs(x);
	
	if (x < 1.0) {
		float x2 = x * x;
		float x3 = x * x2;
		return (21 * x3 - 36 * x2 + 16) / 18;
	}
	
	if (x < 2.0) {
		float x2 = x * x;
		float x3 = x * x2;
		return (-7 * x3 + 36 * x2 - 60 * x + 32) / 18;
	}
	
	return 0.0;
}

[numthreads(16, 8, 1)]
void Resize(uint3 id: SV_DispatchThreadID) {
	int2 inSize, outSize;
	input.GetDimensions(inSize.x, inSize.y);
	output.GetDimensions(outSize.x, outSize.y);
	
	float2 scale = float2(outSize) / inSize;
	float2 size = 2.0 / scale;
	
	float2 center = (float2(id.xy) + 0.5) / scale;
	int2 start = clamp(int2(center - size), int2(0, 0), inSize - int2(1, 1));
	int2 end = clamp(int2(center + size), int2(0, 0), inSize - int2(1, 1));
	
	float weight = 0;
	float3 color = float3(0, 0, 0);
	for (int y = start.y; y <= end.y; y++) {
		float dy = (center.y - y - 0.5) * scale.y;
		float fy = Filter(dy);
		
		for (int x = start.x; x <= end.x; x++) {
			float dx = (center.x - x - 0.5) * scale.x;
			float fx = Filter(dx);
			
			float w = fx * fy;
			color += input[int2(x, y)] * w;
			weight += w;
		}
	}
	
	if (weight > 0) color /= weight;
	
	// packs float3 Color to uint BGRA output
	output[id.xy] = dot(uint3(clamp(color.bgr, 0, 1) * 255 + 0.5), uint3(1, 1 << 8, 1 << 16));
}

float3 RgbToYuv(float3 rgb) {
	return mul(convertMtx, float4(rgb, 1.0));
}

[numthreads(16, 8, 1)]
void Convert(uint3 id: SV_DispatchThreadID) {
	int2 inSize;
	input.GetDimensions(inSize.x, inSize.y);
	
	int2 pos = int2(id.xy);
	int2 pos2 = pos * 2;
	int4 pos4 = int4(pos2, pos2 + int2(1, 1));
	
	int4 src = int4(pos2, min(pos4.zw, inSize - int2(1, 1)));
	
	// load RGB colors in 2x2 area
	float3 rgb0 = input[src.xy];
	float3 rgb1 = input[src.zy];
	float3 rgb2 = input[src.xw];
	float3 rgb3 = input[src.zw];
	
	// convert RGB to YUV
	float3 yuv0 = RgbToYuv(rgb0);
	float3 yuv1 = RgbToYuv(rgb1);
	float3 yuv2 = RgbToYuv(rgb2);
	float3 yuv3 = RgbToYuv(rgb3);
	
	// average UV
	float2 uv = (yuv0.yz + yuv1.yz + yuv2.yz + yuv3.yz) / 4.0;
	
	// store Y
	outputY[pos4.xy] = uint(yuv0.x);
	outputY[pos4.zy] = uint(yuv1.x);
	outputY[pos4.xw] = uint(yuv2.x);
	outputY[pos4.zw] = uint(yuv3.x);
	
	// store UV
	outputUV[pos] = uint2(uv);
}