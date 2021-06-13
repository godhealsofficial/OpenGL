#version 330 core
in vec3 vFragPosition;
in vec2 vTexCoords;
in vec3 vNormal;
in mat3 TBN;

out vec4 result_color;

struct Material {
	sampler2D texture_diffuse;
	vec3 diffuse_color;

	
    sampler2D normalMap; // карта нормалей
	vec3 speculare_color;
    float shininess;
};

struct PointLight {
	vec3 position;

	vec3 color;
	vec3 ambient;
	vec3 specular;
	float mult;
};

struct DirectedLight {
	vec3 direction;


	vec3 color;
	vec3 ambient;
	vec3 specular;
	float mult;	 //множитель
};

#define POINT_LIGHTS_NUM 2
uniform PointLight pointLights[POINT_LIGHTS_NUM];

#define DIRECTED_LIGHTS_NUM 1
uniform DirectedLight directedLights[DIRECTED_LIGHTS_NUM];

uniform Material material;
uniform bool typeOfTexture;
uniform vec3 camPos;

float calcDiffuse(vec3 norm, vec3 lightDir)
{
	return max(dot(norm, lightDir), 0.0);
}

vec3 lightContribDirected(DirectedLight light, vec3 normal, vec3 fragPos, vec3 viewDir)
{
	vec3 lightDir = normalize(-light.direction);
	float diff = max(dot(normal, lightDir), 0.0);

	vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);


	vec3 ambient = light.ambient * vec3(texture(material.texture_diffuse, vTexCoords)).rgb;
	vec3 diffuse = light.color * diff * vec3(texture(material.texture_diffuse, vTexCoords)).rgb;
	vec3 specular = light.specular * spec * vec3(texture(material.texture_diffuse, vTexCoords)).rgb;


	vec3 r = light.mult * (specular + diffuse + ambient);
    return (r);
}


vec3 lightContrib(PointLight light, Material mat, vec3 normal, vec3 fragPos, vec3 viewDir)
{
	vec3 lightDir = normalize(light.position - fragPos);
	float diff = max(dot(normal, lightDir), 0.0);

	vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
	
	float distance = length(light.position - fragPos);
	float attenuation = 1.0f / (distance * distance);

	vec3 ambient = light.ambient * vec3(texture(material.texture_diffuse, vTexCoords)).rgb;
	vec3 diffuse = light.color * diff * vec3(texture(material.texture_diffuse, vTexCoords)).rgb;
	vec3 specular = light.specular * spec * vec3(texture(material.texture_diffuse, vTexCoords)).rgb;

	ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;

	vec3 r = light.mult * (specular + diffuse + ambient);
    return (r);
}

vec3 lightContribNorm(PointLight light, Material mat, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 tangentFragPos)
{
	vec3 tangentLightPos = TBN * light.position;
    vec3 lightDir = normalize(tangentLightPos - tangentFragPos);
	float diff = max(dot(normal, lightDir), 0.0);

	vec3 reflectDir = reflect(- lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
	
	float distance = length(light.position - fragPos);
	float attenuation = 1.0f / (distance * distance);

	vec3 ambient = light.ambient * vec3(texture(material.texture_diffuse, vTexCoords)).rgb;
	vec3 diffuse = light.color * diff * vec3(texture(material.texture_diffuse, vTexCoords)).rgb;
	vec3 specular = light.specular * spec * vec3(texture(material.texture_diffuse, vTexCoords)).rgb;

	ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;

	vec3 r = light.mult * (specular + diffuse + ambient);
    return (r);
}

void main()
{

	vec3 viewDir = vec3(0, 0, 0); //TODO: read camera parameter
	result_color = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	if (!typeOfTexture) {
           	for (int i = 0; i < POINT_LIGHTS_NUM; ++i)
		         result_color += vec4(lightContrib(pointLights[i], material, vNormal, vFragPosition, viewDir), 1.0f);

            for (int i = 0; i < DIRECTED_LIGHTS_NUM; ++i)
		         result_color += vec4(lightContribDirected(directedLights[i], vNormal, vFragPosition, viewDir), 1.0f);	
		
		} else {
		    vec3 normal = texture(material.normalMap, vTexCoords).rgb;
            normal = normalize(normal * 2.0 - 1.0); // перевод вектора нормали в интервал [-1,1]

			vec3 tangentViewPos  = TBN * camPos;
            vec3 tangentFragPos  = TBN * vFragPosition;

            vec3 viewDir = normalize(tangentViewPos - tangentFragPos);

			for (int i = 0; i < POINT_LIGHTS_NUM; ++i)
	    	result_color += vec4(lightContribNorm(pointLights[i], material, normal, vFragPosition, viewDir, tangentFragPos), 1.0f);

            for (int i = 0; i < DIRECTED_LIGHTS_NUM; ++i)
	    	result_color += vec4(lightContribDirected(directedLights[i], normal, vFragPosition, viewDir), 1.0f);
		}
}
