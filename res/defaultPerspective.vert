#version 330 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 texCoord;

uniform mat4 modelCFrame;
uniform mat4 modelRotation;
uniform mat4 cameraInverse;
uniform mat4 projection;

uniform vec4 modelSize;

out vec2 texCoords;

out vec3 faceNormal;
out vec3 vertexPosition;
out vec3 barycentric;

void main() {
    vertexPosition = (modelCFrame * (vec4(position, 1.0f) * modelSize)).xyz;
    faceNormal = normalize((modelRotation * (vec4(normal.xy, -normal.z, 1.0f) * modelSize)).xyz);
    texCoords = texCoord;

	float m = mod(gl_VertexID, 3);
	barycentric = vec3(
		1.0 - abs(sign(m)),
		1.0 - abs(sign(m - 1.0f)),
		1.0 - abs(sign(m - 2.0f))
	);

    gl_Position = projection * cameraInverse * vec4(vertexPosition.xyz, 1.0f);
}