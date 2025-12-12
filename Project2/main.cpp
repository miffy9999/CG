#define _CRT_SECURE_NO_WARNINGS

#include <GL/glew.h>
#include <GL/freeglut.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <iostream>
#include <algorithm> 
#include <fstream> // [Room 2 추가] 파일 입출력

using namespace std;
using namespace glm;

// [Room 2 추가] 텍스처 경로 및 퍼즐 변수
const char* textureFilePath = "Data/wood.bmp";
bool isPuzzleClear = false;

// [윈도우 설정]
int windowWidth = 1000;
int windowHeight = 800;

bool isLevelClear = false;

// -------------------------------------------------------
// [Room 2 추가] BMP 로더 함수 (기존 코드 영향 없음)
// -------------------------------------------------------
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

// -------------------------------------------------------
// [기본 오브젝트 클래스] (기존 유지)
// -------------------------------------------------------
class GameObject {
public:
    vec3 position;
    vec3 scale;
    vec3 rotation;
    vec3 color;

    // [물리 변수]
    vec3 velocity;
    vec3 force;
    float mass;
    bool isStatic;

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

    virtual void Draw() = 0;
};

// -------------------------------------------------------
// [큐브] (기존 유지)
// -------------------------------------------------------
class Cube : public GameObject {
public:
    Cube(vec3 pos, vec3 sz, vec3 col) : GameObject(pos, sz, col) {
        mass = 5.0f;
    }

    void Draw() override {
        glPushMatrix();
        glTranslatef(position.x, position.y, position.z);
        glRotatef(rotation.y, 0, 1, 0);
        glScalef(scale.x, scale.y, scale.z);
        glColor3f(color.r, color.g, color.b);
        glutSolidCube(1.0f);
        glPopMatrix();
    }
};

// -------------------------------------------------------
// [구체] (기존 유지)
// -------------------------------------------------------
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
    }
};

// -------------------------------------------------------
// [구멍 뚫린 벽] (기존 유지)
// -------------------------------------------------------
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

        // 로컬 좌표계 기준 부품 생성
        parts[0] = { vec3(0, (hh + topH) / 2.0f, 0), vec3(w, topH, thick) }; // 상
        parts[1] = { vec3(0, -(hh + topH) / 2.0f, 0), vec3(w, topH, thick) }; // 하
        parts[2] = { vec3(-(hw + sideW) / 2.0f, 0, 0), vec3(sideW, hh, thick) }; // 좌
        parts[3] = { vec3((hw + sideW) / 2.0f, 0, 0), vec3(sideW, hh, thick) }; // 우
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
};

// -------------------------------------------------------
// [버튼] (기존 유지)
// -------------------------------------------------------
class Button {
public:
    vec3 position;
    bool isPressed;
    GameObject* targetObj;

    Button(vec3 pos, GameObject* target) : position(pos), targetObj(target), isPressed(false) {}

    void Update() {
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

// -------------------------------------------------------
// [Room 2 추가] 아나모픽 퍼즐 클래스 (기존 클래스 상속 X, 독립적 운영)
// -------------------------------------------------------
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
        // [위치 조정] Room 1의 뒤쪽 공간(Z < -20)에 배치
        projectorPos = vec3(12.0f, 4.0f, -32.0f);
        lookAtTarget = vec3(0.0f, 5.0f, -50.0f);
        texID = 0;
    }

    void Init(const char* texturePath) {
        float spreadX = 13.0f; float spreadY = 9.0f; float spreadZ = 13.0f;
        float centerY = 5.0f; float centerZ = -45.0f;
        float minSize = 1.0f; float maxSize = 2.0f;

        int w, h;
        unsigned char* data = LoadBMP(texturePath, &w, &h);
        if (data) {
            glGenTextures(1, &texID);
            glBindTexture(GL_TEXTURE_2D, texID);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
            delete[] data;
        }

        pieces.clear();
        for (int i = 0; i < 100; i++) {
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
    }

    vec2 GetProjectedUV(vec3 worldPos) {
        mat4 view = lookAt(projectorPos, lookAtTarget, vec3(0, 1, 0));
        mat4 proj = perspective(radians(45.0f), (float)800 / 600, 0.1f, 100.0f);
        vec4 clipSpace = proj * view * vec4(worldPos, 1.0f);
        vec3 ndc = vec3(clipSpace) / clipSpace.w;
        return vec2(ndc.x * 0.5f + 0.5f, ndc.y * 0.5f + 0.5f);
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

            // 단순 큐브 그리기 + UV 매핑
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

// -------------------------------------------------------
// [전역 변수]
// -------------------------------------------------------
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
        Yaw += x * 0.1f; Pitch += y * 0.1f;
        if (Pitch > 89.0f) Pitch = 89.0f; if (Pitch < -89.0f) Pitch = -89.0f;
        UpdateVectors();
    }
    void ProcessKey(int dir, bool clear) {
        float vel = 0.5f;
        vec3 f = normalize(vec3(Front.x, 0, Front.z));
        vec3 r = normalize(cross(f, Up));
        vec3 next = Pos;
        if (dir == 0) next += f * vel; if (dir == 1) next -= f * vel;
        if (dir == 2) next -= r * vel; if (dir == 3) next += r * vel;

        if (next.x > 19) next.x = 19; if (next.x < -19) next.x = -19;
        if (next.y < 4) next.y = 4;

        // [Room 2 수정] 이동 제한 로직 변경
        if (!clear) {
            // 클리어 전: 기존 제한 유지
            if (next.z > 19) next.z = 19;
            if (next.z < -19.0f) next.z = -19.0f;
        }
        else {
            // 클리어 후: Room 2(-60)까지 이동 허용
            if (next.z > 19) next.z = 19;
            if (next.z < -59.0f) next.z = -59.0f; // Room 2 끝

            // 문 통과 로직 (문 주변 벽 충돌)
            bool insideDoorZone = (next.z < -19.0f && next.z > -21.0f);
            if (insideDoorZone && abs(next.x) > 2.0f) {
                next.z = Pos.z; // 벽에 막힘
            }
        }
        Pos = next;
    }
};

Camera mainCamera(vec3(0.0f, 6.0f, 15.0f));

// 오브젝트 포인터들
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

// [Room 2 추가] 객체 포인터
AnamorphicPuzzle myPuzzle;
Cube* room2Floor;
Cube* room2Back;
Cube* room2Left;
Cube* room2Right;

GameObject* heldObject = nullptr;
float grabDistance = 0.0f;
vec3 grabOriginalScale;

// -------------------------------------------------------
// [충돌 및 유틸 함수]
// -------------------------------------------------------
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

    // 법선 계산 (간단 버전)
    float tminX = std::min(t1, t2); float tminY = std::min(t3, t4); float tminZ = std::min(t5, t6);
    float finalMin = std::max(std::max(tminX, tminY), tminZ);

    if (finalMin == tminX) hitNormal = vec3((rayOrigin.x < box.min.x) ? -1 : 1, 0, 0);
    else if (finalMin == tminY) hitNormal = vec3(0, (rayOrigin.y < box.min.y) ? -1 : 1, 0);
    else hitNormal = vec3(0, 0, (rayOrigin.z < box.min.z) ? -1 : 1);

    return true;
}

// [Room 2 수정] 발판 판정 확장
float GetFloorHeightAt(vec3 pos) {
    if (pos.x < -19.0f && pos.x > -22.0f && pos.z > -2.0f && pos.z < 2.0f) return 5.5f;
    if (pos.x > 19.0f && pos.x < 22.0f && pos.z > -2.0f && pos.z < 2.0f) return 5.5f;

    // Room 2 바닥 체크 (추가됨)
    if (pos.z < -20.0f && pos.z > -60.0f) return 0.0f;

    return 0.0f;
}

void InitObjects() {
    myCube = new Cube(vec3(5, 5, 5), vec3(2, 2, 2), vec3(0.8f, 0.6f, 0.4f));
    myCube->isStatic = false;
    mySphere = new Sphere(vec3(-5, 5, 5), vec3(2, 2, 2), vec3(0.2f, 0.6f, 1.0f));
    mySphere->isStatic = false;
    floorObj = new Cube(vec3(0, -0.5, 0), vec3(40, 1, 40), vec3(0.8f, 0.8f, 0.8f));

    // [구멍 벽] 내부에서 collisionCubes 생성됨
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

    // [Room 2 추가] 객체 초기화 및 퍼즐 로드
    room2Floor = new Cube(vec3(0, -0.5, -40), vec3(40, 1, 40), vec3(0.6f, 0.6f, 0.6f));
    room2Back = new Cube(vec3(0, 7.5, -60), vec3(40, 15, 2), vec3(0.7f, 0.7f, 0.7f));
    room2Left = new Cube(vec3(-20, 7.5, -40), vec3(2, 15, 40), vec3(0.7f, 0.7f, 0.7f));
    room2Right = new Cube(vec3(20, 7.5, -40), vec3(2, 15, 40), vec3(0.7f, 0.7f, 0.7f));
    myPuzzle.Init(textureFilePath);
}

void UpdateGame() {
    btnLeft->Update();
    btnRight->Update();
    isLevelClear = (btnLeft->isPressed && btnRight->isPressed);

    // [Room 2 추가] 퍼즐 체크
    if (!isPuzzleClear && isLevelClear && myPuzzle.CheckSolved(mainCamera.Pos)) {
        cout << "Puzzle Clear!" << endl;
        isPuzzleClear = true;
    }
}

void DrawScene() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();

    vec3 target = mainCamera.Pos + mainCamera.Front;
    gluLookAt(mainCamera.Pos.x, mainCamera.Pos.y, mainCamera.Pos.z,
        target.x, target.y, target.z, mainCamera.Up.x, mainCamera.Up.y, mainCamera.Up.z);

    // [드로잉] 모든 객체를 Draw 함수로 그리기
    floorObj->Draw();
    backWall->Draw();
    ceilingObj->Draw();
    frontWallLeft->Draw();
    frontWallRight->Draw();
    frontDoorTop->Draw();

    if (!isLevelClear) exitDoor->Draw();

    leftWall->Draw(); btnLeft->Draw();
    rightWall->Draw(); btnRight->Draw();

    // [Room 2 추가] 렌더링
    room2Floor->Draw(); room2Back->Draw();
    room2Left->Draw(); room2Right->Draw();
    myPuzzle.Draw();

    // 힌트 (빨간 공)
    if (isLevelClear && !isPuzzleClear) {
        glPushMatrix();
        glTranslatef(myPuzzle.projectorPos.x, myPuzzle.projectorPos.y, myPuzzle.projectorPos.z);
        glColor3f(1, 0, 0); glutWireSphere(0.3f, 10, 10);
        glPopMatrix();
    }

    if (heldObject) {
        float minDist = 10000.0f;

        // [충돌 대상 목록]
        vector<GameObject*> obstacles;
        if (heldObject != myCube) obstacles.push_back(myCube);
        if (heldObject != mySphere) obstacles.push_back(mySphere);

        // 바닥, 뒷벽, 천장, 앞벽 등등 모두 추가
        obstacles.push_back(floorObj);
        obstacles.push_back(backWall);
        obstacles.push_back(ceilingObj);
        obstacles.push_back(frontWallLeft);
        obstacles.push_back(frontWallRight);
        obstacles.push_back(frontDoorTop);
        obstacles.push_back(exitDoor);

        // [Room 2 추가] 벽 충돌 포함
        obstacles.push_back(room2Floor); obstacles.push_back(room2Back);
        obstacles.push_back(room2Left); obstacles.push_back(room2Right);

        // [핵심] WallWithHole은 통째로 넣지 않고, 내부의 큐브들을 넣는다
        for (auto* p : leftWall->collisionCubes) obstacles.push_back(p);
        for (auto* p : rightWall->collisionCubes) obstacles.push_back(p);

        // 레이캐스트 및 충돌 처리
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

        // 바닥/천장 무한 평면 처리 (혹시 박스 사이로 샜을 경우 대비)
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

    myCube->Draw();
    mySphere->Draw();

    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity(); glOrtho(0, windowWidth, 0, windowHeight, -1, 1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity(); glDisable(GL_LIGHTING); glDisable(GL_DEPTH_TEST);
    glColor3f(1, 1, 1); glBegin(GL_LINES);
    glVertex2f(windowWidth / 2 - 10, windowHeight / 2); glVertex2f(windowWidth / 2 + 10, windowHeight / 2);
    glVertex2f(windowWidth / 2, windowHeight / 2 - 10); glVertex2f(windowWidth / 2, windowHeight / 2 + 10); glEnd();
    glEnable(GL_DEPTH_TEST); glEnable(GL_LIGHTING); glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(GL_MODELVIEW);

    glutSwapBuffers();
}

void MyTimer(int val) {
    UpdateGame();
    if (heldObject != myCube) myCube->UpdatePhysics(0.02f, GetFloorHeightAt(myCube->position));
    if (heldObject != mySphere) mySphere->UpdatePhysics(0.02f, GetFloorHeightAt(mySphere->position));
    glutPostRedisplay();
    glutTimerFunc(16, MyTimer, 0);
}

void MyMouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        if (heldObject) heldObject = nullptr;
        else {
            GameObject* objs[] = { myCube, mySphere };
            float minD = 1000.0f;
            GameObject* picked = nullptr;
            for (auto obj : objs) {
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
    int cx = windowWidth / 2; int cy = windowHeight / 2;
    if (x == cx && y == cy) return;
    mainCamera.ProcessMouse(x - cx, cy - y);
    glutWarpPointer(cx, cy);
    glutPostRedisplay();
}

void MyKeyboard(unsigned char key, int x, int y) {
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
    glutCreateWindow("Merged Project");

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