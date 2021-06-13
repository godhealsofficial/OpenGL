#version 330 core
layout(location = 0) in vec3 vertex;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoords;
layout(location = 4) in vec3 tangent;
layout(location = 5) in vec3 bitangent;


out vec2 vTexCoords;
out vec3 vFragPosition;
out vec3 vNormal;
out mat3 TBN;


uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform int  type;

uniform vec3 offsets[100];

void main()
{

  if(type == 2){
       vec3 offset = offsets[gl_InstanceID];

       gl_Position = projection * view * model * (vec4(vertex, 1.0f) + vec4(offset, 0.0f));
       vTexCoords = texCoords;

       vFragPosition = vec3(model * (vec4(vertex, 1.0f))) + vec3(offset);
       vNormal = normalize(mat3(transpose(inverse(model))) * normal);
       
  } else {
      gl_Position = projection * view * model * vec4(vertex, 1.0f);

       vTexCoords = texCoords;

       vFragPosition = vec3(model * vec4(vertex, 1.0f));
       vNormal = normalize(mat3(transpose(inverse(model))) * normal);
  }

         mat3 normalMatrix = transpose(inverse(mat3(model)));

         vec3 T = normalize(normalMatrix * tangent);
         vec3 N = normalize(normalMatrix * vNormal);
         T = normalize(T - dot(T, N) * N);
         vec3 B = cross(N, T);

         TBN = transpose(mat3(T, B, N));

}