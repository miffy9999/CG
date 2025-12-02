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
// [기본 오브젝트 클래스] - 복잡한 물리 속성 제거
// -------------------------------------------------------
class GameObject {
public:
    vec3 position;
    vec3 scale;
    vec3 rotation;
    vec3 color;

    // [간단한 물리 변수]
    vec3 velocity;
    vec3 force;
    float mass;
    bool isStatic; // 고정 물체 여부

    GameObject(vec3 pos, vec3 sz, vec3 col)
        : position(pos), scale(sz), rotation(0.0f, 0.0f, 0.0f), color(col),
        velocity(0.0f), force(0.0f), mass(1.0f), isStatic(true) {
    }

    virtual ~GameObject() {}

    // [학습한 내용 적용: 간단한 물리 업데이트]
    // 탄성, 마찰 계수 없이 "바닥에 닿으면 멈춤"으로 단순화
    void UpdatePhysics(float dt, float currentFloorY) {
        if (isStatic) return;

        // 1. 중력 적용 (F = m * g)
        // 원본 코드의 개념대로 중력만 추가
        vec3 gravity(0.0f, -20.0f, 0.0f);
        force += gravity * mass;

        // 2. 공기 저항 (원본 코드에 있던 감쇠력 개념 - v * 0.01)
        force -= velocity * 0.1f;

        // 3. 적분 (힘 -> 가속도 -> 속도 -> 위치)
        vec3 accel = force / mass;
        velocity += accel * dt;
        position += velocity * dt;

        // 4. 바닥 충돌 처리 (단순 제약 조건)
        // 복잡한 튕김(Elastic) 계산 없이, 바닥 뚫는 것만 방지
        float halfHeight = scale.y / 2.0f;
        float bottomY = position.y - halfHeight;

        if (bottomY < currentFloorY) {
            // 위치: 바닥 위로 올림
            position.y = currentFloorY + halfHeight;

            // 속도: 바닥 방향 속도가 있다면 0으로 만듦 (안 튕기고 그냥 멈춤)
            if (velocity.y < 0) {
                velocity.y = 0.0f;
                // 바닥에서는 수평 속도도 빠르게 줄어들게 하여 멈추게 함 (간이 마찰)
                velocity.x *= 0.9f;
                velocity.z *= 0.9f;
            }
        }

        // 힘 초기화
        force = vec3(0.0f);
    }

    virtual void Draw() = 0;
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
// [구멍 뚫린 벽]
// -------------------------------------------------------
class WallWithHole : public GameObject {
public:
    vec3 holeSize;
    struct SubWall { vec3 pos; vec3 scale; };
    SubWall parts[5];

    WallWithHole(vec3 pos, vec3 wallSz, vec3 hSz, vec3 col, float rotY)
        : GameObject(pos, wallSz, col), holeSize(hSz)
    {
        rotation.y = rotY;

        float thick = wallSz.z;
        float w = wallSz.x; float h = wallSz.y;
        float hw = hSz.x;   float hh = hSz.y;

        float topH = (h - hh) / 2.0f;
        float sideW = (w - hw) / 2.0f;

        // 상, 하, 좌, 우
        parts[0] = { vec3(0, (hh + topH) / 2.0f, 0), vec3(w, topH, thick) };
        parts[1] = { vec3(0, -(hh + topH) / 2.0f, 0), vec3(w, topH, thick) };
        parts[2] = { vec3(-(hw + sideW) / 2.0f, 0, 0), vec3(sideW, hh, thick) };
        parts[3] = { vec3((hw + sideW) / 2.0f, 0, 0), vec3(sideW, hh, thick) };

        // 뒷판 (구멍 뒤 막기)
        parts[4] = { vec3(0, 0, -thick), vec3(hw, hh, thick / 2) };
    }

    void Draw() override {
        glPushMatrix();
        glTranslatef(position.x, position.y, position.z);
        glRotatef(rotation.y, 0, 1, 0);

        glColor3f(color.r, color.g, color.b);
        for (int i = 0; i < 4; i++) {
            glPushMatrix();
            glTranslatef(parts[i].pos.x, parts[i].pos.y, parts[i].pos.z);
            glScalef(parts[i].scale.x, parts[i].scale.y, parts[i].scale.z);
            glutSolidCube(1.0f);
            glPopMatrix();
        }

        // 뒷판
        glColor3f(color.r * 0.7f, color.g * 0.7f, color.b * 0.7f);
        glPushMatrix();
        glTranslatef(parts[4].pos.x, parts[4].pos.y, parts[4].pos.z);
        glScalef(parts[4].scale.x, parts[4].scale.y, parts[4].scale.z);
        glutSolidCube(1.0f);
        glPopMatrix();

        glPopMatrix();
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
        float dist = distance(vec3(position.x, 0, position.z), vec3(targetObj->position.x, 0, targetObj->position.z));
        bool onTop = (targetObj->position.y - (targetObj->scale.y / 2.0f)) < (position.y + 0.5f);
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

Cube* myCube;
Sphere* mySphere;
WallWithHole* leftWall;
WallWithHole* rightWall;
Cube* exitDoor;
Cube* floorObj;
Button* btnLeft;
Button* btnRight;

GameObject* heldObject = nullptr;
float grabDistance = 0.0f;
vec3 grabOriginalScale;

// -------------------------------------------------------
// [로직 함수]
// -------------------------------------------------------
float GetRayDistance() {
    float minT = 1000.0f;
    struct Plane { vec3 p; vec3 n; };
    vector<Plane> planes = {
        {vec3(0,0,-20), vec3(0,0,1)}, {vec3(0,0,20), vec3(0,0,-1)},
        {vec3(-20,0,0), vec3(1,0,0)}, {vec3(20,0,0), vec3(-1,0,0)},
        {vec3(0,0,0), vec3(0,1,0)},   {vec3(0,15,0), vec3(0,-1,0)}
    };
    for (auto& pl : planes) {
        float denom = dot(pl.n, mainCamera.Front);
        if (abs(denom) > 0.0001f) {
            float t = dot(pl.p - mainCamera.Pos, pl.n) / denom;
            if (t > 0.5f && t < minT) minT = t;
        }
    }
    return minT - 2.0f;
}

float GetFloorHeightAt(vec3 pos) {
    // 왼쪽 구멍 (X : -22 ~ -18)
    if (pos.x < -18.0f && pos.x > -22.0f && pos.z > -2.0f && pos.z < 2.0f) return 5.5f;
    // 오른쪽 구멍 (X : 18 ~ 22)
    if (pos.x > 18.0f && pos.x < 22.0f && pos.z > -2.0f && pos.z < 2.0f) return 5.5f;
    return 0.0f;
}

void InitObjects() {
    myCube = new Cube(vec3(5, 5, 5), vec3(2, 2, 2), vec3(0.8f, 0.6f, 0.4f));
    myCube->isStatic = false;

    mySphere = new Sphere(vec3(-5, 5, 5), vec3(2, 2, 2), vec3(0.2f, 0.6f, 1.0f));
    mySphere->isStatic = false;

    floorObj = new Cube(vec3(0, -0.5, 0), vec3(40, 1, 40), vec3(0.8f, 0.8f, 0.8f));

    // 왼쪽 벽: 90도 회전
    leftWall = new WallWithHole(vec3(-20, 7.5, 0), vec3(40, 15, 2), vec3(4, 4, 4), vec3(0.7f, 0.7f, 0.7f), 90.0f);

    // [수정됨] 오른쪽 벽: -90도 회전 (구멍 방향 수정)
    rightWall = new WallWithHole(vec3(20, 7.5, 0), vec3(40, 15, 2), vec3(4, 4, 4), vec3(0.7f, 0.7f, 0.7f), -90.0f);

    exitDoor = new Cube(vec3(0, 5, -20), vec3(8, 10, 1), vec3(0.3f, 0.0f, 0.0f));

    btnLeft = new Button(vec3(-19.0f, 5.5f, 0.0f), mySphere);
    btnRight = new Button(vec3(19.0f, 5.5f, 0.0f), myCube);
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

    floorObj->Draw();
    glPushMatrix(); glTranslatef(0, 15.5, 0); glScalef(40, 1, 40); glColor3f(0.8f, 0.8f, 0.8f); glutSolidCube(1.0f); glPopMatrix();
    glPushMatrix(); glTranslatef(0, 7.5, 20); glScalef(40, 15, 2); glColor3f(0.7f, 0.7f, 0.7f); glutSolidCube(1.0f); glPopMatrix();

    glPushMatrix(); glTranslatef(-12, 7.5, -20); glScalef(16, 15, 2); glutSolidCube(1.0f); glPopMatrix();
    glPushMatrix(); glTranslatef(12, 7.5, -20); glScalef(16, 15, 2); glutSolidCube(1.0f); glPopMatrix();
    glPushMatrix(); glTranslatef(0, 12.5, -20); glScalef(8, 5, 2); glutSolidCube(1.0f); glPopMatrix();

    if (!isLevelClear) exitDoor->Draw();

    leftWall->Draw(); btnLeft->Draw();
    rightWall->Draw(); btnRight->Draw();

    if (heldObject) {
        float dist = GetRayDistance();
        float scaleRatio = dist / grabDistance;
        heldObject->position = mainCamera.Pos + (mainCamera.Front * dist);
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