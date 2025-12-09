#define _CRT_SECURE_NO_WARNINGS

#include <GL/glew.h>
#include <GL/freeglut.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <iostream>
#include <algorithm> 

using namespace std;
using namespace glm;

// [윈도우 설정]
int windowWidth = 1000;
int windowHeight = 800;

bool isLevelClear = false;

// -------------------------------------------------------
// [기본 오브젝트 클래스]
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
// [큐브]
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
// [구체]
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
// [구멍 뚫린 벽] - 수정됨 (뒷판 유격 제거)
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

        // [수정 핵심] 뒷판 위치 조정 (유격 제거)
        // 벽의 두께(thick)는 2.0. 벽의 뒷면은 로컬 Z = -1.0.
        // 뒷판의 두께는 1.0. 뒷판의 중심을 -1.5로 두어야 앞면이 -1.0이 되어 딱 붙음.
        // 즉, -thick * 0.75f 위치에 두면 됨.
        parts[4] = { vec3(0, 0, -thick * 0.75f), vec3(hw, hh, thick / 2.0f) };

        // OpenGL 좌표계 회전 방향 반전 (-rotY)
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
// [버튼]
// -------------------------------------------------------
class Button {
public:
    vec3 position;
    bool isPressed;
    GameObject* targetObj;

    Button(vec3 pos, GameObject* target) : position(pos), targetObj(target), isPressed(false) {}

    void Update() {
        // XZ 평면 거리 체크
        float dist = distance(vec3(position.x, 0, position.z), vec3(targetObj->position.x, 0, targetObj->position.z));
        // 물체가 버튼보다 위에 있는지 체크
        bool onTop = (targetObj->position.y - (targetObj->scale.y / 2.0f)) < (position.y + 0.5f);
        // 물체 크기가 너무 작거나 크지 않은지 체크 (적당한 크기만 인정)
        bool sizeMatch = (targetObj->scale.x > 1.0f && targetObj->scale.x < 3.8f);

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
        if (next.z > 19) next.z = 19; if (next.y < 4) next.y = 4;

        if (next.z < -19.0f) {
            if (!clear) next.z = -19.0f;
            else if (abs(next.x) > 2.0f) next.z = -19.0f;
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

// [추가] 유령이었던 벽들을 실체화
Cube* backWall;
Cube* ceilingObj;
Cube* frontWallLeft;
Cube* frontWallRight;
Cube* frontDoorTop;

Cube* exitDoor;
Cube* floorObj;
Button* btnLeft;
Button* btnRight;

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

// [수정] 발판(Platform) 판정 범위 조정
// 벽의 실제 두께와 뒷판 위치를 고려하여 정확히 벽 위에만 서도록 조정
float GetFloorHeightAt(vec3 pos) {
    // 왼쪽 벽 (X: -20, 두께 2 => -19~-21) + 뒷판(두께1 => -21~-22)
    // 따라서 발판은 대략 -19 ~ -22 범위
    if (pos.x < -19.0f && pos.x > -22.0f && pos.z > -2.0f && pos.z < 2.0f) return 5.5f;

    // 오른쪽 벽 (X: 20, 두께 2 => 19~21) + 뒷판(두께1 => 21~22)
    // 따라서 발판은 대략 19 ~ 22 범위
    if (pos.x > 19.0f && pos.x < 22.0f && pos.z > -2.0f && pos.z < 2.0f) return 5.5f;

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

    // [추가] 하드코딩 되어있던 벽들을 실제 객체로 생성
    backWall = new Cube(vec3(0, 7.5, 20), vec3(40, 15, 2), vec3(0.7f, 0.7f, 0.7f));
    ceilingObj = new Cube(vec3(0, 15.5, 0), vec3(40, 1, 40), vec3(0.8f, 0.8f, 0.8f));

    // 앞쪽 벽들 (문 주변)
    frontWallLeft = new Cube(vec3(-12, 7.5, -20), vec3(16, 15, 2), vec3(0.7f, 0.7f, 0.7f));
    frontWallRight = new Cube(vec3(12, 7.5, -20), vec3(16, 15, 2), vec3(0.7f, 0.7f, 0.7f));
    frontDoorTop = new Cube(vec3(0, 12.5, -20), vec3(8, 5, 2), vec3(0.7f, 0.7f, 0.7f));

    exitDoor = new Cube(vec3(0, 5, -20), vec3(8, 10, 1), vec3(0.3f, 0.0f, 0.0f));

    // [수정] 버튼 위치를 구멍 뚫린 벽의 두께 정중앙(20.0f)으로 이동
    btnLeft = new Button(vec3(-20.0f, 5.5f, 0.0f), mySphere);
    btnRight = new Button(vec3(20.0f, 5.5f, 0.0f), myCube);
}

void UpdateGame() {
    btnLeft->Update();
    btnRight->Update();
    isLevelClear = (btnLeft->isPressed && btnRight->isPressed);
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
    // [단순화된 물리] 
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
    glutCreateWindow("Simple Physics Project");

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
