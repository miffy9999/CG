#define _CRT_SECURE_NO_WARNINGS

#include <GL/glew.h>
#include <GL/freeglut.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <iostream>
#include <algorithm>
#include <fstream>
#include <deque>
#include <utility>
#include <math.h>
#include <time.h>


#define NUM_PARTICLES    4000    
#define NUM_DEBRIS       1000    

struct particleData {
    float position[3];
    float speed[3];
    float color[3];
};

struct debrisData {
    float position[3];
    float speed[3];
    float orientation[3];
    float orientationSpeed[3];
    float color[3];
    float scale[3];
};

using namespace std;
using namespace glm;


const char* textureFilePath = "../Data/Cube.bmp";
bool isPuzzleClear = false;


int windowWidth = 1000;
int windowHeight = 800;

bool isLevelClear = false;
particleData particles[NUM_PARTICLES];
debrisData debris[NUM_DEBRIS];
int fuel = 0;                

bool isRoom2Exploded = false; 
bool isRoom1Exploded = false; 
int explosionSequenceTimer = 0; 

enum GameState {
    STATE_NORMAL,       
    STATE_TRANSITION,  
    STATE_EXPLODED      
};

GameState currentState = STATE_NORMAL;
float transitionTime = 0.0f; 


vec3 startCamPos, startCamTarget, startCamUp;
vec3 endCamPos(0.0f, 80.0f, -20.0f);     
vec3 endCamTarget(0.0f, 0.0f, -20.0f);   
vec3 endCamUp(0.0f, 0.0f, -1.0f);        

GLuint skyTextureID; 


unsigned char* LoadBMP(const char* filename, int* width, int* height) {
    FILE* file = fopen(filename, "rb");
    if (!file) return NULL;
    unsigned char header[54];
    if (fread(header, 1, 54, file) != 54) { fclose(file); return NULL; }
    *width = *(int*)&(header[0x12]);
    *height = *(int*)&(header[0x16]);
    int imageSize = *(int*)&(header[0x22]);
    if (imageSize == 0) imageSize = (*width) * (*height) * 3;
    unsigned char* data = new unsigned char[imageSize];
    fread(data, 1, imageSize, file);
    fclose(file);
    for (int i = 0; i < imageSize - 2; i += 3) {
        unsigned char temp = data[i]; data[i] = data[i + 2]; data[i + 2] = temp;
    }
    return data;
}
GLuint LoadTexture(const char* filename) {
    int w, h;
    unsigned char* data = LoadBMP(filename, &w, &h);
    if (!data) {
        return 0;
    }
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    delete[] data;
    return id;
}

void InitSkybox() {
    int w, h;
    unsigned char* data = LoadBMP("../Data/Sky.bmp", &w, &h);

    if (data) {
        glGenTextures(1, &skyTextureID);
        glBindTexture(GL_TEXTURE_2D, skyTextureID);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        delete[] data;
        cout << "Skybox Texture Loaded!" << endl;
    }
    else {
        cout << "Failed to load Skybox Texture!" << endl;
    }
}


class GameObject {
public:
    vec3 position;
    vec3 scale;
    vec3 rotation;
    vec3 color;

    vec3 velocity;
    vec3 force;
    float mass;
    bool isStatic;

    deque<pair<vec3, vec3>> trails;

    GameObject(vec3 pos, vec3 sz, vec3 col)
        : position(pos), scale(sz), rotation(0.0f, 0.0f, 0.0f), color(col),
        velocity(0.0f), force(0.0f), mass(1.0f), isStatic(true) {
    }

    virtual ~GameObject() {}

    void UpdatePhysics(float dt, float currentFloorY) {
        if (isStatic) return;

        vec3 gravity(0.0f, -20.0f, 0.0f);
        force += gravity * mass;
        force -= velocity * 0.1f;

        vec3 accel = force / mass;
        velocity += accel * dt;
        position += velocity * dt;

        float halfHeight = scale.y / 2.0f;
        float bottomY = position.y - halfHeight;

        if (bottomY < currentFloorY) {
            position.y = currentFloorY + halfHeight;
            if (velocity.y < 0) {
                velocity.y = 0.0f;
                velocity.x *= 0.9f;
                velocity.z *= 0.9f;
            }
        }
        force = vec3(0.0f);
    }

    void UpdateTrails(bool isHeld) {
        if (isHeld) {
            trails.push_front({ position, scale }); 
            if (trails.size() > 6) trails.pop_back(); 
        }
        else {
            // 움직임이 멈추면 잔상이 서서히 사라짐
            if (!trails.empty()) trails.pop_back();
        }
    }

    virtual void Draw() = 0;
    virtual void DrawShadow(float* shadowMat) = 0;
};


class Cube : public GameObject {
public:
    GLuint texIDs[3];
    bool hasTexture;

    Cube(vec3 pos, vec3 sz, vec3 col) : GameObject(pos, sz, col) {
        mass = 5.0f;
        texIDs[0] = 0; texIDs[1] = 0; texIDs[2] = 0;
        hasTexture = false;
    }

    void SetTextures(const char* file1, const char* file2, const char* file3) {
        texIDs[0] = LoadTexture(file1); 
        texIDs[1] = LoadTexture(file2); 
        texIDs[2] = LoadTexture(file3); 

        if (texIDs[0] != 0) hasTexture = true;
    }

    void Draw() override {
        glPushMatrix();
        glTranslatef(position.x, position.y, position.z);

        glRotatef(rotation.x, 1, 0, 0);
        glRotatef(rotation.y, 0, 1, 0);
        glRotatef(rotation.z, 0, 0, 1);

        glScalef(scale.x, scale.y, scale.z);

        if (hasTexture) {
           glDisable(GL_LIGHTING);

            glEnable(GL_TEXTURE_2D);
            glColor3f(1.0f, 1.0f, 1.0f); 

            
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

            float s = 0.5f;

            if (texIDs[0]) glBindTexture(GL_TEXTURE_2D, texIDs[0]);
            glBegin(GL_QUADS);
            glNormal3f(0, 0, 1);  glTexCoord2f(0, 0); glVertex3f(-s, -s, s); glTexCoord2f(1, 0); glVertex3f(s, -s, s); glTexCoord2f(1, 1); glVertex3f(s, s, s); glTexCoord2f(0, 1); glVertex3f(-s, s, s);
            glNormal3f(0, 0, -1); glTexCoord2f(0, 0); glVertex3f(-s, -s, -s); glTexCoord2f(1, 0); glVertex3f(-s, s, -s); glTexCoord2f(1, 1); glVertex3f(s, s, -s); glTexCoord2f(0, 1); glVertex3f(s, -s, -s);
            glEnd();

            if (texIDs[1]) glBindTexture(GL_TEXTURE_2D, texIDs[1]);
            glBegin(GL_QUADS);
            glNormal3f(0, 1, 0);  glTexCoord2f(0, 0); glVertex3f(-s, s, -s); glTexCoord2f(1, 0); glVertex3f(-s, s, s); glTexCoord2f(1, 1); glVertex3f(s, s, s); glTexCoord2f(0, 1); glVertex3f(s, s, -s);
            glNormal3f(0, -1, 0); glTexCoord2f(0, 0); glVertex3f(-s, -s, -s); glTexCoord2f(1, 0); glVertex3f(-s, -s, s); glTexCoord2f(1, 1); glVertex3f(s, -s, s); glTexCoord2f(0, 1); glVertex3f(s, -s, -s);
            glEnd();

            if (texIDs[2]) glBindTexture(GL_TEXTURE_2D, texIDs[2]);
            glBegin(GL_QUADS);
            glNormal3f(-1, 0, 0); glTexCoord2f(0, 0); glVertex3f(-s, -s, -s); glTexCoord2f(1, 0); glVertex3f(-s, -s, s); glTexCoord2f(1, 1); glVertex3f(-s, s, s); glTexCoord2f(0, 1); glVertex3f(-s, s, -s);
            glNormal3f(1, 0, 0);  glTexCoord2f(0, 0); glVertex3f(s, -s, -s); glTexCoord2f(1, 0); glVertex3f(s, -s, s); glTexCoord2f(1, 1); glVertex3f(s, s, s); glTexCoord2f(0, 1); glVertex3f(s, s, -s);
            glEnd();

            glDisable(GL_TEXTURE_2D);

            glEnable(GL_LIGHTING);
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        }
        else {
            glColor3f(color.r, color.g, color.b);
            glutSolidCube(1.0f);
        }

        glPopMatrix();
        
        if (!trails.empty()) {
            glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            float alpha = 0.5f;
            for (auto& t : trails) {
                glPushMatrix();
                glTranslatef(t.first.x, t.first.y, t.first.z);
                glRotatef(rotation.x, 1, 0, 0); glRotatef(rotation.y, 0, 1, 0); glRotatef(rotation.z, 0, 0, 1);
                glScalef(t.second.x, t.second.y, t.second.z);
                glColor4f(0.8f, 0.6f, 0.4f, alpha);
                glutSolidCube(1.0f);
                glPopMatrix();
                alpha -= 0.05f;
            }
            glDisable(GL_BLEND);
        }
    }

    void DrawShadow(float* shadowMat) override {
        if (!shadowMat) return;
        glPushMatrix();
        glMultMatrixf(shadowMat);
        glTranslatef(position.x, position.y, position.z);
        glRotatef(rotation.x, 1, 0, 0);
        glRotatef(rotation.y, 0, 1, 0);
        glRotatef(rotation.z, 0, 0, 1);
        glScalef(scale.x, scale.y, scale.z);
        glDisable(GL_LIGHTING);
        glColor3f(0.0f, 0.0f, 0.0f);
        glutSolidCube(1.0f);
        glEnable(GL_LIGHTING);
        glPopMatrix();
    }
};


class Sphere : public GameObject {
public:
    Sphere(vec3 pos, vec3 sz, vec3 col) : GameObject(pos, sz, col) {
        mass = 2.0f;
    }

    void Draw() override {
        glPushMatrix();
        glTranslatef(position.x, position.y, position.z);
        glScalef(scale.x, scale.y, scale.z);
        glColor3f(color.r, color.g, color.b);
        glutSolidSphere(0.5f, 32, 32);
        glPopMatrix();

        if (!trails.empty()) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            float alpha = 0.5f;
            for (auto& t : trails) {
                glPushMatrix();
                glTranslatef(t.first.x, t.first.y, t.first.z);
                glScalef(t.second.x, t.second.y, t.second.z);
                glColor4f(0.5f, 0.8f, 1.0f, alpha);
                glutSolidSphere(0.5f, 16, 16);
                glPopMatrix();
                alpha -= 0.05f;
            }
            glDisable(GL_BLEND);
        }
    }

    void DrawShadow(float* shadowMat) override {
        if (!shadowMat) return;

        glPushMatrix();
        glMultMatrixf(shadowMat);
        glTranslatef(position.x, position.y, position.z);
        glScalef(scale.x, scale.y, scale.z);
        glDisable(GL_LIGHTING);
        glColor3f(0.0f, 0.0f, 0.0f);
        glutSolidSphere(0.5f, 32, 32);
        glEnable(GL_LIGHTING);
        glPopMatrix();
    }
};


class WallWithHole : public GameObject {
public:
    struct SubWall { vec3 pos; vec3 scale; };
    SubWall parts[5];
    vector<Cube*> collisionCubes;

    WallWithHole(vec3 pos, vec3 wallSz, vec3 hSz, vec3 col, float rotY)
        : GameObject(pos, wallSz, col)
    {
        rotation.y = rotY;

        float thick = wallSz.z;
        float w = wallSz.x; float h = wallSz.y;
        float hw = hSz.x;   float hh = hSz.y;

        float topH = (h - hh) / 2.0f;
        float sideW = (w - hw) / 2.0f;

        parts[0] = { vec3(0, (hh + topH) / 2.0f, 0), vec3(w, topH, thick) }; 
        parts[1] = { vec3(0, -(hh + topH) / 2.0f, 0), vec3(w, topH, thick) }; 
        parts[2] = { vec3(-(hw + sideW) / 2.0f, 0, 0), vec3(sideW, hh, thick) }; 
        parts[3] = { vec3((hw + sideW) / 2.0f, 0, 0), vec3(sideW, hh, thick) }; 
        parts[4] = { vec3(0, 0, -thick * 0.75f), vec3(hw, hh, thick / 2.0f) };

        float rad = radians(-rotY);
        float cosR = cos(rad);
        float sinR = sin(rad);

        for (int i = 0; i < 5; i++) {
            float worldX = pos.x + (parts[i].pos.x * cosR - parts[i].pos.z * sinR);
            float worldZ = pos.z + (parts[i].pos.x * sinR + parts[i].pos.z * cosR);
            float worldY = pos.y + parts[i].pos.y;

            float scaleX = abs(parts[i].scale.x * cosR) + abs(parts[i].scale.z * sinR);
            float scaleZ = abs(parts[i].scale.x * sinR) + abs(parts[i].scale.z * cosR);

            Cube* c = new Cube(vec3(worldX, worldY, worldZ), vec3(scaleX, parts[i].scale.y, scaleZ), col);
            if (i == 4) c->color = vec3(col.r * 0.7f, col.g * 0.7f, col.b * 0.7f);

            collisionCubes.push_back(c);
        }
    }

    void Draw() override {
        for (Cube* c : collisionCubes) {
            c->Draw();
        }
    }

    void DrawShadow(float* shadowMat) {}
};


class Button {
public:
    vec3 position;
    bool isPressed;
    GameObject* targetObj;

    Button(vec3 pos, GameObject* target) : position(pos), targetObj(target), isPressed(false) {}

    void Update() {
        if (!targetObj) { isPressed = false; return; }

        float dist = distance(vec3(position.x, 0, position.z), vec3(targetObj->position.x, 0, targetObj->position.z));
        bool onTop = (targetObj->position.y - (targetObj->scale.y / 2.0f)) < (position.y + 0.5f);
        bool sizeMatch = (targetObj->scale.x > 0.7f && targetObj->scale.x < 4.0f);

        if (dist < 1.5f && onTop && sizeMatch) isPressed = true;
        else isPressed = false;
    }

    void Draw() {
        glPushMatrix();
        glTranslatef(position.x, position.y + 0.1f, position.z);
        glScalef(2.0f, 0.2f, 2.0f);
        if (isPressed) {
            glColor3f(0.0f, 1.0f, 0.0f);
            float em[] = { 0, 0.8f, 0, 1 }; glMaterialfv(GL_FRONT, GL_EMISSION, em);
        }
        else {
            glColor3f(1.0f, 0.0f, 0.0f);
            float em[] = { 0.8f, 0, 0, 1 }; glMaterialfv(GL_FRONT, GL_EMISSION, em);
        }
        glutSolidCube(1.0f);
        float noEm[] = { 0, 0, 0, 1 }; glMaterialfv(GL_FRONT, GL_EMISSION, noEm);
        glPopMatrix();
    }
};


class AnamorphicPuzzle {
public:
    struct Piece {
        vec3 pos;
        vec3 scale;
        vec3 rot;
    };
    vector<Piece> pieces;

    vec3 projectorPos;
    vec3 lookAtTarget;
    GLuint texID;

    AnamorphicPuzzle() {
        projectorPos = vec3(12.0f, 6.0f, -32.0f);
        lookAtTarget = vec3(-6.0f, 4.0f, -50.0f);
        texID = 0;
    }

    void AddPiece(vec3 position, float size) {
        Piece p;
        p.pos = position;
        p.scale = vec3(size, size, size);
        p.rot = vec3(rand() % 360, rand() % 360, rand() % 360);
        pieces.push_back(p);
    }

    void Init(const char* texturePath) {
        float spreadX = 10.0f; float spreadY = 10.0f; float spreadZ = 9.0f;
        float centerY = 5.0f; float centerZ = -45.0f;
        float minSize = 1.0f; float maxSize = 2.0f;


        int w, h;
        unsigned char* data = LoadBMP(texturePath, &w, &h);
        if (data) {
            glGenTextures(1, &texID);
            glBindTexture(GL_TEXTURE_2D, texID);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
            delete[] data;
        }

        pieces.clear();
        for (int i = 0; i < 70; i++) {
            Piece p;
            float r1 = (rand() % 1000) / 1000.0f;
            float r2 = (rand() % 1000) / 1000.0f;
            float r3 = (rand() % 1000) / 1000.0f;
            float r4 = (rand() % 1000) / 1000.0f;

            float rx = (r1 * spreadX) - (spreadX / 2.0f);
            float ry = (r2 * spreadY) - (spreadY / 2.0f) + centerY;
            float rz = (r3 * spreadZ) - (spreadZ / 2.0f) + centerZ;

            p.pos = vec3(rx, ry, rz);
            float scale = minSize + (r4 * (maxSize - minSize));
            p.scale = vec3(scale, scale, scale);
            p.rot = vec3(rand() % 360, rand() % 360, rand() % 360);
            pieces.push_back(p);
        }
        AddPiece(vec3(-2.9436f, 5.79062f, -41.6549f), 1.5f);
        AddPiece(vec3(-2.98785f, 7.13326f, -41.3439f), 1.5f);
        AddPiece(vec3(-5.13883f, 3.2476f, -43.1836f), 1.5f);
        AddPiece(vec3(-6.21343f, 5.00316f, -43.7882f), 1.5f);
        AddPiece(vec3(-3.85938f, 4.98606f, -42.4979f), 1.5f);
        AddPiece(vec3(-2.17921f, 1.01903f, -44.6914f), 1.5f);
        AddPiece(vec3(-0.230905f, 1.00076f, -44.5743f), 1.5f);
        AddPiece(vec3(-2.88306f, 8.65699f, -48.3253f), 1.5f);
        AddPiece(vec3(-2.315f, 6.79079f, -48.488f), 1.5f);
        AddPiece(vec3(1.83649f, 6.7563f, -43.5348f), 1.5f);
    }

    vec2 GetProjectedUV(vec3 worldPos) {
        mat4 view = lookAt(projectorPos, lookAtTarget, vec3(0, 1, 0));
        mat4 proj = perspective(radians(30.0f), 1.0f, 0.1f, 100.0f);
        vec4 clipSpace = proj * view * vec4(worldPos, 1.0f);
        vec3 ndc = vec3(clipSpace) / clipSpace.w;

        float u = 1.0f - (ndc.x * 0.5f + 0.5f);
        float v = ndc.y * 0.5f + 0.5f;

        return vec2(u, v);
    }

    void Draw() {
        if (texID == 0) return;
        glDisable(GL_LIGHTING);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, texID);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

        for (const auto& p : pieces) {
            glPushMatrix();
            glTranslatef(p.pos.x, p.pos.y, p.pos.z);
            glRotatef(p.rot.x, 1, 0, 0); glRotatef(p.rot.y, 0, 1, 0); glRotatef(p.rot.z, 0, 0, 1);
            float s = p.scale.x / 2.0f;

            glBegin(GL_QUADS);
            vec3 v[8] = { {-s,-s, s}, { s,-s, s}, { s, s, s}, {-s, s, s}, {-s,-s,-s}, { s,-s,-s}, { s, s,-s}, {-s, s,-s} };
            int faces[6][4] = { {0,1,2,3}, {5,4,7,6}, {4,0,3,7}, {1,5,6,2}, {3,2,6,7}, {4,5,1,0} };

            mat4 model = mat4(1.0f);
            model = translate(model, p.pos);
            model = rotate(model, radians(p.rot.x), vec3(1, 0, 0));
            model = rotate(model, radians(p.rot.y), vec3(0, 1, 0));
            model = rotate(model, radians(p.rot.z), vec3(0, 0, 1));

            for (int i = 0; i < 6; i++) {
                for (int j = 0; j < 4; j++) {
                    vec3 localPos = v[faces[i][j]];
                    vec4 worldPos4 = model * vec4(localPos, 1.0f);
                    vec2 uv = GetProjectedUV(vec3(worldPos4));
                    glTexCoord2f(uv.x, uv.y);
                    glVertex3f(localPos.x, localPos.y, localPos.z);
                }
            }
            glEnd();
            glPopMatrix();
        }
        glDisable(GL_TEXTURE_2D);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        glEnable(GL_LIGHTING);
    }

    bool CheckSolved(vec3 playerPos) {
        return (distance(playerPos, projectorPos) < 2.0f);
    }
};


class Camera {
public:
    vec3 Pos, Front, Up;
    float Yaw = -90.0f, Pitch = 0.0f;
    Camera(vec3 pos) : Pos(pos), Front(0, 0, -1), Up(0, 1, 0) {}
    void UpdateVectors() {
        vec3 front;
        front.x = cos(radians(Yaw)) * cos(radians(Pitch));
        front.y = sin(radians(Pitch));
        front.z = sin(radians(Yaw)) * cos(radians(Pitch));
        Front = normalize(front);
    }
    void ProcessMouse(float x, float y) {
        if (isRoom2Exploded) return; 

        Yaw += x * 0.1f; Pitch += y * 0.1f;
        if (Pitch > 89.0f) Pitch = 89.0f; if (Pitch < -89.0f) Pitch = -89.0f;
        UpdateVectors();
    }
    void ProcessKey(int dir, bool clear) {
        if (isRoom2Exploded) return; 

        float vel = 0.5f;
        vec3 f = normalize(vec3(Front.x, 0, Front.z));
        vec3 r = normalize(cross(f, Up));
        vec3 next = Pos;
        if (dir == 0) next += f * vel; if (dir == 1) next -= f * vel;
        if (dir == 2) next -= r * vel; if (dir == 3) next += r * vel;

        if (next.x > 19) next.x = 19; if (next.x < -19) next.x = -19;
        if (next.y < 4) next.y = 4;

        
        if (!clear) {
            
            if (next.z > 19) next.z = 19;
            if (next.z < -19.0f) next.z = -19.0f;
        }
        else {
            
            if (next.z > 19) next.z = 19;
            if (next.z < -59.0f) next.z = -59.0f; 

           
            bool insideDoorZone = (next.z < -19.0f && next.z > -21.0f);
            if (insideDoorZone && abs(next.x) > 2.0f) {
                next.z = Pos.z; 
            }
        }
        Pos = next;
    }
};

Camera mainCamera(vec3(0.0f, 6.0f, 15.0f));


Cube* myCube;
Sphere* mySphere;
WallWithHole* leftWall;
WallWithHole* rightWall;

Cube* backWall;
Cube* ceilingObj;
Cube* frontWallLeft;
Cube* frontWallRight;
Cube* frontDoorTop;

Cube* exitDoor;
Cube* floorObj;
Button* btnLeft;
Button* btnRight;


AnamorphicPuzzle myPuzzle;
Cube* room2Floor;
Cube* room2Back;
Cube* room2Left;
Cube* room2Top;
WallWithHole* room2RightHole;
Button* btnRoom2;
Cube* rotatedBox;

GameObject* heldObject = nullptr;
float grabDistance = 0.0f;
vec3 grabOriginalScale;


struct AABB { vec3 min; vec3 max; };

AABB GetAABB(GameObject* obj) {
    vec3 halfSize = obj->scale / 2.0f;
    return { obj->position - halfSize, obj->position + halfSize };
}

bool IntersectRayAABB(vec3 rayOrigin, vec3 rayDir, AABB box, vec3& hitNormal, float& hitDist) {
    vec3 invDir = 1.0f / rayDir;
    float t1 = (box.min.x - rayOrigin.x) * invDir.x;
    float t2 = (box.max.x - rayOrigin.x) * invDir.x;
    float t3 = (box.min.y - rayOrigin.y) * invDir.y;
    float t4 = (box.max.y - rayOrigin.y) * invDir.y;
    float t5 = (box.min.z - rayOrigin.z) * invDir.z;
    float t6 = (box.max.z - rayOrigin.z) * invDir.z;

    float tmin = std::max(std::max(std::min(t1, t2), std::min(t3, t4)), std::min(t5, t6));
    float tmax = std::min(std::min(std::max(t1, t2), std::max(t3, t4)), std::max(t5, t6));

    if (tmax < 0 || tmin > tmax) return false;

    hitDist = tmin;

    
    float tminX = std::min(t1, t2); float tminY = std::min(t3, t4); float tminZ = std::min(t5, t6);
    float finalMin = std::max(std::max(tminX, tminY), tminZ);

    if (finalMin == tminX) hitNormal = vec3((rayOrigin.x < box.min.x) ? -1 : 1, 0, 0);
    else if (finalMin == tminY) hitNormal = vec3(0, (rayOrigin.y < box.min.y) ? -1 : 1, 0);
    else hitNormal = vec3(0, 0, (rayOrigin.z < box.min.z) ? -1 : 1);

    return true;
}


bool CheckOverlapXZ(vec3 posA, vec3 scaleA, vec3 posB, vec3 scaleB) {
    float halfAX = scaleA.x / 2.0f; float halfAZ = scaleA.z / 2.0f;
    float halfBX = scaleB.x / 2.0f; float halfBZ = scaleB.z / 2.0f;

    bool collisionX = (posA.x - halfAX < posB.x + halfBX) && (posA.x + halfAX > posB.x - halfBX);
    bool collisionZ = (posA.z - halfAZ < posB.z + halfBZ) && (posA.z + halfAZ > posB.z - halfBZ);

    return collisionX && collisionZ;
}


float GetFloorHeightAt(vec3 pos, vec3 scale) {
    float height = 0.0f; 


    if (pos.x < -19.0f && pos.x > -22.0f && pos.z > -2.0f && pos.z < 2.0f) height = 5.5f;
    else if (pos.x > 19.0f && pos.x < 22.0f && pos.z > -2.0f && pos.z < 2.0f) height = 5.5f;


    else if (pos.z < -20.0f && pos.z > -60.0f) height = 0.0f;

    
    vector<Cube*> wallParts;

    if (leftWall) for (auto c : leftWall->collisionCubes) wallParts.push_back(c);
    if (rightWall) for (auto c : rightWall->collisionCubes) wallParts.push_back(c);

    if (room2RightHole) for (auto c : room2RightHole->collisionCubes) wallParts.push_back(c);

    for (auto c : wallParts) {
        if (CheckOverlapXZ(pos, scale, c->position, c->scale)) {

           float topY = c->position.y + (c->scale.y / 2.0f);

            float objBottom = pos.y - (scale.y / 2.0f);

            if (objBottom >= topY - 1.0f) {
                if (topY > height) height = topY;
            }
        }
    }

    return height;
}

void newSpeed(float dest[3]) {
    float x, y, z, len;
    x = (2.0 * ((GLfloat)rand()) / ((GLfloat)RAND_MAX)) - 1.0;
    y = (2.0 * ((GLfloat)rand()) / ((GLfloat)RAND_MAX)) - 1.0;
    z = (2.0 * ((GLfloat)rand()) / ((GLfloat)RAND_MAX)) - 1.0;
    len = sqrt(x * x + y * y + z * z);
    if (len) { x /= len; y /= len; z /= len; }
    dest[0] = x * 1.5f; dest[1] = y * 1.5f; dest[2] = z * 1.5f;
}

void newExplosion(vec3 pos, int startIdx, int pCount, int dStartIdx, int dCount) {
    for (int i = startIdx; i < startIdx + pCount; i++) {
        particles[i].position[0] = pos.x;
        particles[i].position[1] = pos.y;
        particles[i].position[2] = pos.z;

        int colorType = rand() % 3;
        if (colorType == 0) { 
            particles[i].color[0] = 1.0f; particles[i].color[1] = 1.0f; particles[i].color[2] = 0.8f;
        }
        else if (colorType == 1) { 
            particles[i].color[0] = 1.0f; particles[i].color[1] = 0.5f; particles[i].color[2] = 0.0f;
        }
        else {
            particles[i].color[0] = 1.0f; particles[i].color[1] = 0.0f; particles[i].color[2] = 0.0f;
        }

        newSpeed(particles[i].speed);
    }

    for (int i = dStartIdx; i < dStartIdx + dCount; i++) {
        debris[i].position[0] = pos.x;
        debris[i].position[1] = pos.y;
        debris[i].position[2] = pos.z;

        debris[i].orientation[0] = 0.0; debris[i].orientation[1] = 0.0; debris[i].orientation[2] = 0.0;

        debris[i].color[0] = 0.3f; debris[i].color[1] = 0.3f; debris[i].color[2] = 0.3f;

        debris[i].scale[0] = (2.0 * ((GLfloat)rand()) / ((GLfloat)RAND_MAX)) - 1.0;
        debris[i].scale[1] = (2.0 * ((GLfloat)rand()) / ((GLfloat)RAND_MAX)) - 1.0;
        debris[i].scale[2] = (2.0 * ((GLfloat)rand()) / ((GLfloat)RAND_MAX)) - 1.0;

        newSpeed(debris[i].speed);
        newSpeed(debris[i].orientationSpeed);
    }
    fuel = 500;
}

void UpdateParticles() {
    if (fuel > 0) {
        for (int i = 0; i < NUM_PARTICLES; i++) {
            particles[i].position[0] += particles[i].speed[0] * 0.2f;
            particles[i].position[1] += particles[i].speed[1] * 0.2f;
            particles[i].position[2] += particles[i].speed[2] * 0.2f;
            particles[i].color[0] -= 1.0f / 500.0f; if (particles[i].color[0] < 0) particles[i].color[0] = 0;
            particles[i].color[1] -= 1.0f / 100.0f; if (particles[i].color[1] < 0) particles[i].color[1] = 0;
            particles[i].color[2] -= 1.0f / 50.0f; if (particles[i].color[2] < 0) particles[i].color[2] = 0;
        }
        for (int i = 0; i < NUM_DEBRIS; i++) {
            debris[i].position[0] += debris[i].speed[0] * 0.1f;
            debris[i].position[1] += debris[i].speed[1] * 0.1f;
            debris[i].position[2] += debris[i].speed[2] * 0.1f;
            debris[i].orientation[0] += debris[i].orientationSpeed[0] * 10.0f;
            debris[i].orientation[1] += debris[i].orientationSpeed[1] * 10.0f;
            debris[i].orientation[2] += debris[i].orientationSpeed[2] * 10.0f;
        }
        fuel--; 
    }
}

void InitObjects() {
    InitSkybox();
    myCube = new Cube(vec3(5, 5, 5), vec3(2, 2, 2), vec3(0.8f, 0.6f, 0.4f));
    myCube->isStatic = false;
    mySphere = new Sphere(vec3(-5, 5, 5), vec3(2, 2, 2), vec3(0.2f, 0.6f, 1.0f));
    mySphere->isStatic = false;
    floorObj = new Cube(vec3(0, -0.5, 0), vec3(40, 1, 40), vec3(0.8f, 0.8f, 0.8f));

    leftWall = new WallWithHole(vec3(-20, 7.5, 0), vec3(40, 15, 2), vec3(4, 4, 4), vec3(0.7f, 0.7f, 0.7f), 90.0f);
    rightWall = new WallWithHole(vec3(20, 7.5, 0), vec3(40, 15, 2), vec3(4, 4, 4), vec3(0.7f, 0.7f, 0.7f), -90.0f);

    backWall = new Cube(vec3(0, 7.5, 20), vec3(40, 15, 2), vec3(0.7f, 0.7f, 0.7f));
    ceilingObj = new Cube(vec3(0, 15.5, 0), vec3(40, 1, 40), vec3(0.8f, 0.8f, 0.8f));

    frontWallLeft = new Cube(vec3(-12, 7.5, -20), vec3(16, 15, 2), vec3(0.7f, 0.7f, 0.7f));
    frontWallRight = new Cube(vec3(12, 7.5, -20), vec3(16, 15, 2), vec3(0.7f, 0.7f, 0.7f));
    frontDoorTop = new Cube(vec3(0, 12.5, -20), vec3(8, 5, 2), vec3(0.7f, 0.7f, 0.7f));

    exitDoor = new Cube(vec3(0, 5, -20), vec3(8, 10, 1), vec3(0.3f, 0.0f, 0.0f));

    btnLeft = new Button(vec3(-20.0f, 5.5f, 0.0f), mySphere);
    btnRight = new Button(vec3(20.0f, 5.5f, 0.0f), myCube);

    room2Floor = new Cube(vec3(0, -0.5, -40), vec3(40, 1, 40), vec3(0.8f, 0.8f, 0.8f));
    room2Back = new Cube(vec3(0, 7.5, -60), vec3(40, 15, 2), vec3(0.7f, 0.7f, 0.7f));

    room2Left = new Cube(vec3(-20, 7.5, -40), vec3(2, 15, 40), vec3(0.7f, 0.7f, 0.7f));

    room2RightHole = new WallWithHole(vec3(20, 7.5, -40), vec3(40, 15, 2), vec3(4, 4, 4), vec3(0.7f, 0.7f, 0.7f), -90.0f);

    room2Top = new Cube(vec3(0.0f, 15.5f, -40.0f), vec3(40.0f, 1.0f, 40.0f), vec3(0.6f, 0.6f, 0.6f));


    rotatedBox = new Cube(vec3(1.8f, 5.2f, -42.0f), vec3(4.6f, 4.6f, 4.6f), vec3(1.0f, 1.0f, 1.0f));
    rotatedBox->rotation = vec3(95.0f, 67.0f, 18.0f);
    rotatedBox->isStatic = true;
    rotatedBox->SetTextures("Data/redcube.bmp", "Data/bluecube.bmp", "Data/yellowcube.bmp");

    btnRoom2 = new Button(vec3(20.0f, 5.5f, -40.0f), rotatedBox);

    myPuzzle.Init(textureFilePath);
    srand(time(NULL));
}

void SetShadowMatrix(float* matrix, float* lightPos, float* plane) {
    float dot = plane[0] * lightPos[0] + plane[1] * lightPos[1] + plane[2] * lightPos[2] + plane[3] * lightPos[3];

    matrix[0] = dot - lightPos[0] * plane[0];
    matrix[4] = -lightPos[0] * plane[1];
    matrix[8] = -lightPos[0] * plane[2];
    matrix[12] = -lightPos[0] * plane[3];

    matrix[1] = -lightPos[1] * plane[0];
    matrix[5] = dot - lightPos[1] * plane[1];
    matrix[9] = -lightPos[1] * plane[2];
    matrix[13] = -lightPos[1] * plane[3];

    matrix[2] = -lightPos[2] * plane[0];
    matrix[6] = -lightPos[2] * plane[1];
    matrix[10] = dot - lightPos[2] * plane[2];
    matrix[14] = -lightPos[2] * plane[3];

    matrix[3] = -lightPos[3] * plane[0];
    matrix[7] = -lightPos[3] * plane[1];
    matrix[11] = -lightPos[3] * plane[2];
    matrix[15] = dot - lightPos[3] * plane[3];
}

void UpdateGame() {
    btnLeft->Update();
    btnRight->Update();
    isLevelClear = (btnLeft->isPressed && btnRight->isPressed);

    if (currentState == STATE_NORMAL) {
        btnRoom2->Update();
        if (btnRoom2->isPressed) {
            currentState = STATE_TRANSITION;
            transitionTime = 0.0f;

            startCamPos = mainCamera.Pos;
            startCamTarget = mainCamera.Pos + mainCamera.Front;
            startCamUp = mainCamera.Up;

            heldObject = nullptr;

        }
    }
    else if (currentState == STATE_TRANSITION) {
        transitionTime += 0.005f;

        if (transitionTime >= 1.0f) {
            transitionTime = 1.0f;
            currentState = STATE_EXPLODED;

            isRoom2Exploded = true;
            newExplosion(vec3(0, 5.5, -40), 0, NUM_PARTICLES / 2, 0, NUM_DEBRIS / 2);

            explosionSequenceTimer = 0; 

        }
    }
    else if (currentState == STATE_EXPLODED) {
        explosionSequenceTimer++;

        if (!isRoom1Exploded && explosionSequenceTimer > 120) {
            isRoom1Exploded = true;
            newExplosion(vec3(0, 5.5, 0), NUM_PARTICLES / 2, NUM_PARTICLES / 2, NUM_DEBRIS / 2, NUM_DEBRIS / 2);

        }
    }
}

void DrawSkybox(vec3 pos) {
    if (skyTextureID == 0) return;

    glPushMatrix();

    glTranslatef(pos.x, pos.y, pos.z);

    glDisable(GL_LIGHTING);
    glDepthMask(GL_FALSE);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, skyTextureID);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    float s = 40.0f; 

    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-s, -s, s);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(s, -s, s);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(s, s, s);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-s, s, s);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(-s, -s, -s);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(-s, s, -s);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(s, s, -s);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(s, -s, -s);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-s, s, -s);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-s, s, s);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(s, s, s);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(s, s, -s);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(-s, -s, -s);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(s, -s, -s);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(s, -s, s);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(-s, -s, s);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(s, -s, -s);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(s, s, -s);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(s, s, s);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(s, -s, s);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-s, -s, -s);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(-s, -s, s);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(-s, s, s);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-s, s, -s);
    glEnd();

    glDisable(GL_TEXTURE_2D);
    glDepthMask(GL_TRUE);
    glEnable(GL_LIGHTING);

    glPopMatrix();
}

void RenderText(float x, float y, const char* text, void* font = GLUT_BITMAP_TIMES_ROMAN_24) {
    glRasterPos2f(x, y); 
    for (const char* c = text; *c != '\0'; c++) {
        glutBitmapCharacter(font, *c); 
    }
}

void DrawScene() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();

    vec3 renderPos; 
    if (currentState == STATE_NORMAL) {
        renderPos = mainCamera.Pos; 
        vec3 target = renderPos + mainCamera.Front;
        gluLookAt(renderPos.x, renderPos.y, renderPos.z,
            target.x, target.y, target.z, mainCamera.Up.x, mainCamera.Up.y, mainCamera.Up.z);
    }
    else if (currentState == STATE_TRANSITION) {
        
        float t = transitionTime;
        t = t * t * (3 - 2 * t);

        vec3 curPos = startCamPos * (1.0f - t) + endCamPos * t;
        vec3 curTarget = startCamTarget * (1.0f - t) + endCamTarget * t;
        vec3 curUp = startCamUp * (1.0f - t) + endCamUp * t;

        renderPos = curPos; 

        gluLookAt(curPos.x, curPos.y, curPos.z, curTarget.x, curTarget.y, curTarget.z, curUp.x, curUp.y, curUp.z);
    }
    else if (currentState == STATE_EXPLODED) {
        renderPos = endCamPos; 
        gluLookAt(endCamPos.x, endCamPos.y, endCamPos.z, endCamTarget.x, endCamTarget.y, endCamTarget.z, endCamUp.x, endCamUp.y, endCamUp.z);
    }

    DrawSkybox(renderPos);

    if (!isRoom1Exploded) {
        floorObj->Draw();
        backWall->Draw();
        ceilingObj->Draw();
        frontWallLeft->Draw();
        frontWallRight->Draw();
        frontDoorTop->Draw();

        if (!isLevelClear) exitDoor->Draw();

        leftWall->Draw(); btnLeft->Draw();
        rightWall->Draw(); btnRight->Draw();
    }

    if (!isRoom2Exploded) {
        room2Floor->Draw();
        room2Back->Draw();
        room2Left->Draw();
        room2RightHole->Draw();
        room2Top->Draw();
        if (!isPuzzleClear) {
            myPuzzle.Draw();
        }
        else {
            if (rotatedBox) rotatedBox->Draw();
        }
    }

   if (currentState != STATE_EXPLODED) {
        btnRoom2->Draw();
    }

    if (fuel > 0 && currentState == STATE_EXPLODED) {
        glPushMatrix();
        glDisable(GL_LIGHTING); glDisable(GL_DEPTH_TEST);
        glPointSize(3.0f);
        glBegin(GL_POINTS);
        for (int i = 0; i < NUM_PARTICLES; i++) {
            glColor3fv(particles[i].color);
            glVertex3fv(particles[i].position);
        }
        glEnd();

        glEnable(GL_LIGHTING); glEnable(GL_DEPTH_TEST);
        for (int i = 0; i < NUM_DEBRIS; i++) {
            glColor3fv(debris[i].color);
            glPushMatrix();
            glTranslatef(debris[i].position[0], debris[i].position[1], debris[i].position[2]);
            glRotatef(debris[i].orientation[0], 1, 0, 0);
            glRotatef(debris[i].orientation[1], 0, 1, 0);
            glRotatef(debris[i].orientation[2], 0, 0, 1);
            glScalef(debris[i].scale[0], debris[i].scale[1], debris[i].scale[2]);
            glBegin(GL_TRIANGLES);
            glNormal3f(0.0, 0.0, 1.0);
            glVertex3f(0.0, 0.5, 0.0); glVertex3f(-0.25, 0.0, 0.0); glVertex3f(0.25, 0.0, 0.0);
            glEnd();
            glPopMatrix();
        }
        glPopMatrix();
    }

    if (heldObject && currentState == STATE_NORMAL) {
        float minDist = 10000.0f;

        vector<GameObject*> obstacles;
        if (heldObject != myCube) obstacles.push_back(myCube);
        if (heldObject != mySphere) obstacles.push_back(mySphere);

        obstacles.push_back(floorObj);
        obstacles.push_back(backWall);
        obstacles.push_back(ceilingObj);
        obstacles.push_back(frontWallLeft);
        obstacles.push_back(frontWallRight);
        obstacles.push_back(frontDoorTop);
        obstacles.push_back(exitDoor);

        if (isPuzzleClear && rotatedBox) {
            if (heldObject != rotatedBox) {
                obstacles.push_back(rotatedBox);
            }
        }
        obstacles.push_back(room2Floor); obstacles.push_back(room2Back);
        obstacles.push_back(room2Left);

        if (room2RightHole) {
            for (auto* p : room2RightHole->collisionCubes) {
                obstacles.push_back(p);
            }
        }

        if (leftWall) for (auto* p : leftWall->collisionCubes) obstacles.push_back(p);
        if (rightWall) for (auto* p : rightWall->collisionCubes) obstacles.push_back(p);


        for (auto* obs : obstacles) {
            AABB wallBox = GetAABB(obs);
            vec3 hitNormal;
            float hitT;

            if (IntersectRayAABB(mainCamera.Pos, mainCamera.Front, wallBox, hitNormal, hitT)) {

                float originRadius = 0.0f;
                if (abs(hitNormal.x) > 0.5f) originRadius = grabOriginalScale.x * 0.5f;
                else if (abs(hitNormal.y) > 0.5f) originRadius = grabOriginalScale.y * 0.5f;
                else originRadius = grabOriginalScale.z * 0.5f;

                float cosAngle = abs(dot(mainCamera.Front, hitNormal));
                if (cosAngle < 0.001f) cosAngle = 0.001f;

                if (grabDistance < 0.001f) grabDistance = 0.001f;

                float k = originRadius / (grabDistance * cosAngle);
                float solvedDist = hitT / (1.0f + k);

                solvedDist -= 0.01f;
                if (solvedDist < 0.5f) solvedDist = 0.5f;
                if (solvedDist < minDist) minDist = solvedDist;
            }
        }

        if (mainCamera.Front.y < 0) {
            float t = (0.5f - mainCamera.Pos.y) / mainCamera.Front.y;
            if (t > 0) {
                float originRadius = grabOriginalScale.y * 0.5f;
                float cosAngle = abs(mainCamera.Front.y);
                if (cosAngle < 0.001f) cosAngle = 0.001f;
                float k = originRadius / (grabDistance * cosAngle);
                float solvedDist = t / (1.0f + k);
                if (solvedDist < 0.5f) solvedDist = 0.5f;
                if (solvedDist < minDist) minDist = solvedDist;
            }
        }
        else if (mainCamera.Front.y > 0) {
            float t = (15.0f - mainCamera.Pos.y) / mainCamera.Front.y;
            if (t > 0) {
                float originRadius = grabOriginalScale.y * 0.5f;
                float cosAngle = abs(mainCamera.Front.y);
                if (cosAngle < 0.001f) cosAngle = 0.001f;
                float k = originRadius / (grabDistance * cosAngle);
                float solvedDist = t / (1.0f + k);
                if (solvedDist < 0.5f) solvedDist = 0.5f;
                if (solvedDist < minDist) minDist = solvedDist;
            }
        }

        if (minDist > 1000.0f) minDist = 1000.0f;

        float scaleRatio = minDist / grabDistance;
        heldObject->position = mainCamera.Pos + (mainCamera.Front * minDist);
        heldObject->scale = grabOriginalScale * scaleRatio;
        heldObject->rotation = vec3(0);
        heldObject->velocity = vec3(0);
    }

    float lightPos[] = { 0.0f, 30.0f, 0.0f, 1.0f }; 
    float floorPlane[] = { 0.0f, 1.0f, 0.0f, 0.0f }; 
    float shadowMat[16];
    SetShadowMatrix(shadowMat, lightPos, floorPlane);
    
    glPushMatrix();
    glTranslatef(0.0f, 0.01f, 0.0f);

    if (myCube) myCube->DrawShadow(shadowMat);
    if (mySphere) mySphere->DrawShadow(shadowMat);
    if (isPuzzleClear && rotatedBox && !isRoom2Exploded) rotatedBox->DrawShadow(shadowMat);
    glPopMatrix();

    if (!isRoom1Exploded) {
        myCube->Draw();
        mySphere->Draw();
    }
    
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity(); glOrtho(0, windowWidth, 0, windowHeight, -1, 1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity(); glDisable(GL_LIGHTING); glDisable(GL_DEPTH_TEST);

    if (currentState != STATE_EXPLODED) {
        glColor3f(1, 1, 1); glBegin(GL_LINES);
        glVertex2f(windowWidth / 2 - 10, windowHeight / 2); glVertex2f(windowWidth / 2 + 10, windowHeight / 2);
        glVertex2f(windowWidth / 2, windowHeight / 2 - 10); glVertex2f(windowWidth / 2, windowHeight / 2 + 10); glEnd();
    }

    if (isRoom1Exploded) {
        glColor3f(1.0f, 1.0f, 1.0f);
        RenderText(windowWidth / 2 - 60, windowHeight / 2, "GAME CLEAR");

    }

    glEnable(GL_DEPTH_TEST); glEnable(GL_LIGHTING); glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(GL_MODELVIEW);

    glutSwapBuffers();
}


void MyTimer(int val) {
    UpdateGame();
    UpdateParticles();

    if (heldObject != myCube) {
        myCube->UpdatePhysics(0.02f, GetFloorHeightAt(myCube->position, myCube->scale));
        myCube->UpdateTrails(false);
    }
    else {
        myCube->UpdateTrails(true);
    }

    if (heldObject != mySphere) {
        mySphere->UpdatePhysics(0.02f, GetFloorHeightAt(mySphere->position, mySphere->scale));
        mySphere->UpdateTrails(false);
    }
    else {
        mySphere->UpdateTrails(true);
    }

    if (heldObject != rotatedBox) {
        rotatedBox->UpdatePhysics(0.02f, GetFloorHeightAt(rotatedBox->position, rotatedBox->scale));
        rotatedBox->UpdateTrails(false);
    }
    else {
        rotatedBox->UpdateTrails(true);
    }

    if (isPuzzleClear && rotatedBox) {
        if (heldObject != rotatedBox) {
            rotatedBox->UpdatePhysics(0.02f, GetFloorHeightAt(rotatedBox->position, rotatedBox->scale));
            rotatedBox->UpdateTrails(false);
        }
        else {
            rotatedBox->UpdateTrails(true);
        }
    }

    glutPostRedisplay();
    glutTimerFunc(16, MyTimer, 0);
}

void MyMouse(int button, int state, int x, int y) {
    if (currentState != STATE_NORMAL) return;

    if (!isPuzzleClear && isLevelClear && myPuzzle.CheckSolved(mainCamera.Pos)) {
        cout << "Puzzle Clear! (By Click)" << endl;
        isPuzzleClear = true;
        if (rotatedBox) rotatedBox->isStatic = false;
        mainCamera.Pos = myPuzzle.projectorPos;

        vec3 dir = normalize(myPuzzle.lookAtTarget - mainCamera.Pos);
        mainCamera.Pitch = degrees(asin(dir.y));
        mainCamera.Yaw = degrees(atan2(dir.z, dir.x));

        
        mainCamera.UpdateVectors();

        return;
    }

    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        if (!isPuzzleClear && isLevelClear && myPuzzle.CheckSolved(mainCamera.Pos)) {
            cout << "Puzzle Clear! (By Click)" << endl;
            isPuzzleClear = true;

            if (rotatedBox) rotatedBox->isStatic = false;

            mainCamera.Pos = myPuzzle.projectorPos;
            vec3 dir = normalize(myPuzzle.lookAtTarget - mainCamera.Pos);
            mainCamera.Pitch = degrees(asin(dir.y));
            mainCamera.Yaw = degrees(atan2(dir.z, dir.x));
            mainCamera.UpdateVectors();

            return;
        }
        if (heldObject) heldObject = nullptr;
        else {
            vector<GameObject*> pickCandidates;
            pickCandidates.push_back(myCube);
            pickCandidates.push_back(mySphere);
            if (isPuzzleClear && rotatedBox) {
                pickCandidates.push_back(rotatedBox);
            }
            float minD = 1000.0f;
            GameObject* picked = nullptr;
            for (auto obj : pickCandidates) {
                if (!obj) continue;

                vec3 toObj = obj->position - mainCamera.Pos;
                float d = length(toObj);
                if (d > 30.0f) continue;
                if (dot(mainCamera.Front, normalize(toObj)) > 0.95f) {
                    if (d < minD) { minD = d; picked = obj; }
                }
            }

            if (picked) {
                heldObject = picked;
                grabDistance = distance(mainCamera.Pos, picked->position);
                grabOriginalScale = picked->scale;
            }
        }
    }
}

void MyPassiveMotion(int x, int y) {
    if (currentState != STATE_NORMAL) return; 

    int cx = windowWidth / 2; int cy = windowHeight / 2;
    if (x == cx && y == cy) return;
    mainCamera.ProcessMouse(x - cx, cy - y);
    glutWarpPointer(cx, cy);
    glutPostRedisplay();
}

void MyKeyboard(unsigned char key, int x, int y) {
    if (currentState != STATE_NORMAL && key != 27) return; 

    switch (key) {
    case 'w': mainCamera.ProcessKey(0, isLevelClear); break;
    case 's': mainCamera.ProcessKey(1, isLevelClear); break;
    case 'a': mainCamera.ProcessKey(2, isLevelClear); break;
    case 'd': mainCamera.ProcessKey(3, isLevelClear); break;
    case 27: exit(0); break;
    }
}

void MyReshape(int w, int h) {
    windowWidth = w; windowHeight = h;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    gluPerspective(45.0f, (float)w / h, 0.1f, 100.0f);
    glMatrixMode(GL_MODELVIEW);
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(windowWidth, windowHeight);
    glutCreateWindow("CG Project");

    glewInit();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING); glEnable(GL_LIGHT0); glEnable(GL_COLOR_MATERIAL);
    GLfloat pos[] = { 0, 30, 0, 1 }; glLightfv(GL_LIGHT0, GL_POSITION, pos);

    InitObjects();

    glutSetCursor(GLUT_CURSOR_NONE);
    glutWarpPointer(windowWidth / 2, windowHeight / 2);

    glutDisplayFunc(DrawScene);
    glutReshapeFunc(MyReshape);
    glutKeyboardFunc(MyKeyboard);
    glutPassiveMotionFunc(MyPassiveMotion);
    glutMouseFunc(MyMouse);
    glutTimerFunc(16, MyTimer, 0);

    glutMainLoop();
    return 0;
}
