#version 450 core

// ============================================================================
// BVH CONSTRUCTION COMPUTE SHADER
// Builds a Bounding Volume Hierarchy for fast ray-triangle intersection
// ============================================================================

layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

// ============================================================================
// DATA STRUCTURES
// ============================================================================

struct Triangle {
    vec4 v0; // xyz = position, w = materialID
    vec4 v1;
    vec4 v2;
    vec4 normal; // xyz = face normal, w = area
};

struct BVHNode {
    vec4 aabbMin; // xyz = min bounds, w = leftChild (or -1 if leaf)
    vec4 aabbMax; // xyz = max bounds, w = triangleCount (if leaf)
    int firstTriangle; // Index of first triangle (if leaf)
    int rightChild;    // Index of right child
    int parentNode;    // Index of parent node
    int _padding;
};

// ============================================================================
// SHADER STORAGE BUFFERS
// ============================================================================

// Input: Triangle data from scene
layout(std430, binding = 0) readonly buffer TriangleBuffer {
    Triangle triangles[];
};

// Output: BVH nodes
layout(std430, binding = 1) writeonly buffer BVHBuffer {
    BVHNode nodes[];
};

// Triangle indices for sorting
layout(std430, binding = 2) buffer IndexBuffer {
    int triangleIndices[];
};

// Morton codes for spatial sorting
layout(std430, binding = 3) buffer MortonBuffer {
    uint mortonCodes[];
};

// ============================================================================
// UNIFORMS
// ============================================================================

uniform int u_TriangleCount;
uniform vec3 u_SceneMin;
uniform vec3 u_SceneMax;
uniform int u_BuildPhase; // 0=calc morton, 1=sort, 2=build tree

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

// Calculate AABB for triangle
void GetTriangleBounds(Triangle tri, out vec3 minBounds, out vec3 maxBounds) {
    minBounds = min(min(tri.v0.xyz, tri.v1.xyz), tri.v2.xyz);
    maxBounds = max(max(tri.v0.xyz, tri.v1.xyz), tri.v2.xyz);
}

// Calculate centroid of triangle
vec3 GetTriangleCentroid(Triangle tri) {
    return (tri.v0.xyz + tri.v1.xyz + tri.v2.xyz) / 3.0;
}

// Expand 10-bit integer into 30 bits by inserting 2 zeros after each bit
uint ExpandBits(uint v) {
    v = (v * 0x00010001u) & 0xFF0000FFu;
    v = (v * 0x00000101u) & 0x0F00F00Fu;
    v = (v * 0x00000011u) & 0xC30C30C3u;
    v = (v * 0x00000005u) & 0x49249249u;
    return v;
}

// Calculate 30-bit Morton code from 3D position
uint CalculateMortonCode(vec3 position) {
    // Normalize position to [0, 1024)
    vec3 normalized = (position - u_SceneMin) / (u_SceneMax - u_SceneMin);
    normalized = clamp(normalized * 1024.0, vec3(0.0), vec3(1023.0));
    
    uint x = ExpandBits(uint(normalized.x));
    uint y = ExpandBits(uint(normalized.y));
    uint z = ExpandBits(uint(normalized.z));
    
    return (x * 4) + (y * 2) + z;
}

// Merge two AABBs
void MergeAABB(vec3 min1, vec3 max1, vec3 min2, vec3 max2, 
               out vec3 mergedMin, out vec3 mergedMax) {
    mergedMin = min(min1, min2);
    mergedMax = max(max1, max2);
}

// ============================================================================
// BUILD PHASES
// ============================================================================

// Phase 0: Calculate Morton codes for each triangle
void CalculateMortonCodes() {
    uint index = gl_GlobalInvocationID.x;
    if (index >= u_TriangleCount) return;
    
    Triangle tri = triangles[index];
    vec3 centroid = GetTriangleCentroid(tri);
    
    mortonCodes[index] = CalculateMortonCode(centroid);
    triangleIndices[index] = int(index);
}

// Phase 1: Radix sort (simplified - real impl would use multiple passes)
// This is a placeholder - in production use a proper parallel radix sort
void RadixSort() {
    // Sorting happens on CPU or via multiple compute passes
    // For now, this phase is handled externally
}

// Phase 2: Build BVH tree using LBVH (Linear BVH) algorithm
void BuildTree() {
    uint index = gl_GlobalInvocationID.x;
    if (index >= u_TriangleCount - 1) return; // Internal nodes = n-1
    
    // LBVH: Each thread builds one internal node
    // This is a simplified version - full LBVH is more complex
    
    // Find range of primitives covered by this node
    int first = int(index);
    int last = first + 1;
    
    // Create internal node
    BVHNode node;
    node.leftChild = first;
    node.rightChild = last;
    node.firstTriangle = -1; // Not a leaf
    node.triangleCount = 0;  // Not a leaf
    
    // Calculate AABB from children
    Triangle tri1 = triangles[triangleIndices[first]];
    Triangle tri2 = triangles[triangleIndices[last]];
    
    vec3 min1, max1, min2, max2;
    GetTriangleBounds(tri1, min1, max1);
    GetTriangleBounds(tri2, min2, max2);
    
    vec3 mergedMin, mergedMax;
    MergeAABB(min1, max1, min2, max2, mergedMin, mergedMax);
    
    node.aabbMin = vec4(mergedMin, float(node.leftChild));
    node.aabbMax = vec4(mergedMax, float(node.triangleCount));
    
    nodes[index] = node;
}

// ============================================================================
// MAIN
// ============================================================================

void main() {
    if (u_BuildPhase == 0) {
        CalculateMortonCodes();
    }
    else if (u_BuildPhase == 1) {
        RadixSort();
    }
    else if (u_BuildPhase == 2) {
        BuildTree();
    }
    
    // DEBUG: Write triangle count to first node for verification
    if (gl_GlobalInvocationID.x == 0 && u_BuildPhase == 0) {
        // This helps verify the BVH is being built
        // Can be read back from GPU for debugging
    }
}
