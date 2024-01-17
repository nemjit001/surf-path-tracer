#include "wavefront_common.glsl"

#ifndef GLSL_BVH
#define GLSL_BVH

struct Triangle
{
	vec3 v0;
	vec3 v1;
	vec3 v2;
	vec3 triCentroid;
};

struct TriExtension
{
	vec3 n0, n1, n2;
	vec2 uv0, uv1, uv2;
};

struct BvhNode
{
	uint leftFirst;
	uint count;
	vec3 aabbMin;
	vec3 aabbMax;
};

struct Instance
{
	uint triOffset;
	uint idxOffset;
	uint nodeOffset;
	uint materialOffset;
	float area;
	mat4 transform;
	mat4 invTransform;
};

struct LightData
{
	uint lightInstanceIdx;
	uint primitiveCount;
};

bool bvhNodeIsLeaf(BvhNode node)
{
	return node.count != 0;
}

vec3 scaleVertexBarycentric(Triangle tri, vec2 uv)
{
	return uv.x * tri.v0 + uv.y * tri.v2 + (1.0 - uv.x - uv.y) * tri.v1;
}

vec3 scaleNormalBarycentric(TriExtension ext, vec2 uv)
{
	return uv.x * ext.n0 + uv.y * ext.n2 + (1.0 - uv.x - uv.y) * ext.n1;
}

vec2 scaleTexCoordsBarycentric(TriExtension ext, vec2 uv)
{
	return uv.x * ext.uv0 + uv.y * ext.uv2 + (1.0 - uv.x - uv.y) * ext.uv1;
}

bool triangleIntersect(Triangle tri, inout Ray ray)
{
	vec3 e1 = tri.v1 - tri.v0;
	vec3 e2 = tri.v2 - tri.v0;

	vec3 h = cross(ray.direction, e2);
	float a = dot(e1, h);

	if (abs(a) < F32_EPSILON)
		return false;

	float f = 1.0 / a;
	vec3 s = ray.origin - tri.v0;
	float u = f * dot(s, h);

	if (0.0 > u || u > 1.0)
		return false;

	vec3 q = cross(s, e1);
	float v = f * dot(ray.direction, q);

	if (0.0 > v || (u + v) > 1.0)
		return false;

	float depth = f * dot(e2, q);
	if (!depthInBounds(depth, ray.depth))
		return false;

	ray.depth = depth;
	ray.hit.hitCoords = vec2(u, v);
	return true;
}

float aabbIntersect(BvhNode node, Ray ray)
{
	vec3 rDir = 1.0 / ray.direction;

	float txNear = (node.aabbMin.x - ray.origin.x) * rDir.x;
	float txFar = (node.aabbMax.x - ray.origin.x) * rDir.x;
	float tmin = min(txNear, txFar);
	float tmax = max(txNear, txFar);

	float tyNear = (node.aabbMin.y - ray.origin.y) * rDir.y;
	float tyFar = (node.aabbMax.y - ray.origin.y) * rDir.y;
	tmin = max(tmin, min(tyNear, tyFar));
	tmax = min(tmax, max(tyNear, tyFar));

	float tzNear = (node.aabbMin.z - ray.origin.z) * rDir.z;
	float tzFar = (node.aabbMax.z - ray.origin.z) * rDir.z;
	tmin = max(tmin, min(tzNear, tzFar));
	tmax = min(tmax, max(tzNear, tzFar));

	if (tmax >= tmin && tmin < ray.depth && tmax > 0.0f)
		return tmin;

	return F32_FAR_AWAY;
}

#endif
