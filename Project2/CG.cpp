#define _CRT_SECURE_NO_WARNINGS

#include <GL/glew.h>
#include <GL/freeglut.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm> 

using namespace std;
using namespace glm;

// [사용자 설정: 텍스처 파일 경로]
const char* textureFilePath = "/Data/wood.bmp";

// [윈도우 설정]
int windowWidth = 800;
int windowHeight = 600;

bool isLevelClear = false;      // 클리어 여부

// [충돌 박스 구조체]
struct AABB {
    vec3 min;
    vec3 max;
};

// [모든 물체의 부모 클래스]
class GameObject {
public:
    vec3 position;
    vec3 scale;
    vec3 rotation;

    GameObject(vec3 pos, vec3 sz) : position(pos), scale(sz), rotation(0.0f, 0.0f, 0.0f) {}
    virtual ~GameObject() {}
};

// -------------------------------------------------------
// [추가됨] 플레이어 벽 충돌 체크 함수
// -------------------------------------------------------
vec3 CheckPlayerCollision(vec3 currentPos, vec3 nextPos) {
    // 1. 맵 밖으로 나가는 것 방지
    if (nextPos.x > 19.5f) nextPos.x = 19.5f;
    if (nextPos.x < -19.5f) nextPos.x = -19.5f;
    if (nextPos.z > 19.5f) nextPos.z = 19.5f;
    if (nextPos.y < 2.0f) nextPos.y = 2.0f; // 바닥

    // 2. 앞벽 (Z = -20) 충돌 처리
    // 플레이어가 벽 쪽으로 가려고 할 때(Z가 -19.5보다 작아질 때)
    if (nextPos.z < -19.5f) {
        if (!isLevelClear) {
            // 클리어 전: 절대 통과 불가
            nextPos.z = -19.5f;
        }
        else {
            // 클리어 후: 구멍 위치(X: -1.5 ~ 1.5)가 아니면 막힘
            if (abs(nextPos.x) > 1.5f) {
                nextPos.z = -19.5f;
            }
            // 구멍 위치면 통과 (nextPos 유지)
        }
    }
    return nextPos;
}

class Camera {
public:
    vec3 Pos;
    vec3 Front;
    vec3 Up;
    vec3 Right;
    vec3 WorldUp;

    float Yaw;
    float Pitch;

    float MovementSpeed;
    float MouseSensitivity;

    Camera(vec3 startPos)
        : Pos(startPos), Front(vec3(0.0f, 0.0f, -1.0f)), WorldUp(vec3(0.0f, 1.0f, 0.0f)),
        Yaw(-90.0f), Pitch(0.0f), MovementSpeed(0.5f), MouseSensitivity(0.1f)
    {
        UpdateCameraVectors();
    }

    void ProcessKeyboard(int direction) {
        vec3 frontMove = Front;
        frontMove.y = 0.0f;
        if (length(frontMove) > 0) frontMove = normalize(frontMove);

        float velocity = MovementSpeed;
        vec3 nextPos = Pos; // 이동 예정 위치 계산

        if (direction == 0) nextPos += frontMove * velocity; // W
        if (direction == 1) nextPos -= frontMove * velocity; // S
        if (direction == 2) nextPos -= Right * velocity;     // A
        if (direction == 3) nextPos += Right * velocity;     // D

        // [수정됨] 여기서 충돌 검사를 수행하고 안전한 위치만 적용
        Pos = CheckPlayerCollision(Pos, nextPos);
    }

    void ProcessMouse(float xoffset, float yoffset) {
        xoffset *= MouseSensitivity;
        yoffset *= MouseSensitivity;

        Yaw += xoffset;
        Pitch += yoffset;

        if (Pitch > 89.0f) Pitch = 89.0f;
        if (Pitch < -89.0f) Pitch = -89.0f;

        UpdateCameraVectors();
    }

private:
    void UpdateCameraVectors() {
        vec3 front;
        front.x = cos(radians(Yaw)) * cos(radians(Pitch));
        front.y = sin(radians(Pitch));
        front.z = sin(radians(Yaw)) * cos(radians(Pitch));
        Front = normalize(front);

        Right = normalize(cross(Front, WorldUp));
        Up = normalize(cross(Right, Front));
    }
};

// -------------------------------------------------------
// [1] BMP 로더
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
// [0] 클래스 정의
// -------------------------------------------------------

class Door : public GameObject {
public:
    GLuint texID; // public으로 통일

    // 초기화 시 0으로 설정
    Door(vec3 pos, vec3 sz) : GameObject(pos, sz), texID(0) {}

    void LoadTexture(const char* filename) {
        int width, height;
        unsigned char* data = LoadBMP(filename, &width, &height);

        if (data) {
            glGenTextures(1, &texID); // 멤버 변수 texID에 저장
            glBindTexture(GL_TEXTURE_2D, texID);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);
            delete[] data;
            cout << "Door 텍스처 로드 성공!" << endl;
        }
        else {
            cout << "텍스처 로드 실패!" << endl;
        }
    }

    void Draw() {
        glPushMatrix();

        glTranslatef(position.x, position.y, position.z);
        glRotatef(rotation.y, 0.0f, 1.0f, 0.0f);
        glScalef(scale.x, scale.y, scale.z);

        // 텍스처 입히기 (앞면)
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, texID);
        glColor3f(1.0f, 1.0f, 1.0f);

        glBegin(GL_QUADS);
        glNormal3f(0.0f, 0.0f, 1.0f);
        glTexCoord2f(0.0f, 0.0f); glVertex3f(-0.5f, -0.5f, 0.5f);
        glTexCoord2f(1.0f, 0.0f); glVertex3f(0.5f, -0.5f, 0.5f);
        glTexCoord2f(1.0f, 1.0f); glVertex3f(0.5f, 0.5f, 0.5f);
        glTexCoord2f(0.0f, 1.0f); glVertex3f(-0.5f, 0.5f, 0.5f);
        glEnd();

        // 나머지 면 (단색)
        glDisable(GL_TEXTURE_2D);
        glColor3f(0.4f, 0.2f, 0.0f);

        glBegin(GL_QUADS);
        // 뒤
        glNormal3f(0.0f, 0.0f, -1.0f);
        glVertex3f(-0.5f, -0.5f, -0.5f); glVertex3f(-0.5f, 0.5f, -0.5f);
        glVertex3f(0.5f, 0.5f, -0.5f); glVertex3f(0.5f, -0.5f, -0.5f);
        // 위
        glNormal3f(0.0f, 1.0f, 0.0f);
        glVertex3f(-0.5f, 0.5f, -0.5f); glVertex3f(-0.5f, 0.5f, 0.5f);
        glVertex3f(0.5f, 0.5f, 0.5f); glVertex3f(0.5f, 0.5f, -0.5f);
        // 아래
        glNormal3f(0.0f, -1.0f, 0.0f);
        glVertex3f(-0.5f, -0.5f, -0.5f); glVertex3f(0.5f, -0.5f, -0.5f);
        glVertex3f(0.5f, -0.5f, 0.5f); glVertex3f(-0.5f, -0.5f, 0.5f);
        // 우
        glNormal3f(1.0f, 0.0f, 0.0f);
        glVertex3f(0.5f, -0.5f, -0.5f); glVertex3f(0.5f, 0.5f, -0.5f);
        glVertex3f(0.5f, 0.5f, 0.5f); glVertex3f(0.5f, -0.5f, 0.5f);
        // 좌
        glNormal3f(-1.0f, 0.0f, 0.0f);
        glVertex3f(-0.5f, -0.5f, -0.5f); glVertex3f(-0.5f, -0.5f, 0.5f);
        glVertex3f(-0.5f, 0.5f, 0.5f); glVertex3f(-0.5f, 0.5f, -0.5f);
        glEnd();

        glPopMatrix();
    }
};


class Cube : public GameObject {
public:
	vec3 color; // 큐브 색상 추가

	// 생성자: 위치, 크기, 색상을 받음
	Cube(vec3 pos, vec3 sz, vec3 col) : GameObject(pos, sz), color(col) {}

	void Draw() {
		glPushMatrix();

		// 1. 위치 이동 (GameObject의 position 사용)
		// AABB 계산이 "중심 기준"이므로, 여기서도 중심으로 이동해야 함
		glTranslatef(position.x, position.y, position.z);

		// 2. 크기 계산 (GameObject의 scale 사용)
		// 인자로 w, h, d를 받는 게 아니라 내 몸의 크기(scale)를 써야 함
		float x = scale.x / 2.0f;
		float y = scale.y / 2.0f;
		float z = scale.z / 2.0f;

		glColor3f(color.r, color.g, color.b);

		// 3. 그리기 (중심에서 +/- x, y, z 만큼 뻗어나감)
		glBegin(GL_QUADS);
		// 앞면 (Z축 양의 방향)
		glNormal3f(0, 0, 1);
		glVertex3f(-x, -y, z); glVertex3f(x, -y, z);
		glVertex3f(x, y, z); glVertex3f(-x, y, z);

		// 뒷면 (Z축 음의 방향)
		glNormal3f(0, 0, -1);
		glVertex3f(-x, -y, -z); glVertex3f(-x, y, -z);
		glVertex3f(x, y, -z); glVertex3f(x, -y, -z);

		// 왼쪽 (X축 음의 방향)
		glNormal3f(-1, 0, 0);
		glVertex3f(-x, -y, -z); glVertex3f(-x, -y, z);
		glVertex3f(-x, y, z); glVertex3f(-x, y, -z);

		// 오른쪽 (X축 양의 방향)
		glNormal3f(1, 0, 0);
		glVertex3f(x, -y, -z); glVertex3f(x, y, -z);
		glVertex3f(x, y, z); glVertex3f(x, -y, z);

		// 윗면 (Y축 양의 방향)
		glNormal3f(0, 1, 0);
		glVertex3f(-x, y, -z); glVertex3f(-x, y, z);
		glVertex3f(x, y, z); glVertex3f(x, y, -z);

		// 아랫면 (Y축 음의 방향)
		glNormal3f(0, -1, 0);
		glVertex3f(-x, -y, -z); glVertex3f(x, -y, -z);
		glVertex3f(x, -y, z); glVertex3f(-x, -y, z);
		glEnd();

		glPopMatrix();
	}
};

// [구멍 뚫린 벽 클래스]
class WallWithHole : public GameObject {
public:
	vec3 color;
	vec3 holeSize; // 구멍의 크기 (폭, 높이)
	vec3 holePos;  // 구멍의 위치 (벽 중심 기준 오프셋)

	// 내부적으로 사용할 3개의 큐브 정보를 매번 계산하지 않고 저장해두기 위한 구조체
	struct SubWall { vec3 pos; vec3 scale; };
	SubWall left, right, top;

	WallWithHole(vec3 pos, vec3 wallSz, vec3 hSz, vec3 col): GameObject(pos, wallSz), holeSize(hSz), color(col)
	{
		// 1. 벽의 두께
		float thick = wallSz.z;

		// 2. 전체 벽의 절반 크기
		float halfW = wallSz.x / 2.0f;
		float halfH = wallSz.y / 2.0f;
		float holeHalfW = holeSize.x / 2.0f;

		// ---------------------------------------------------
		// [자동 계산] 3개의 덩어리로 쪼개기
		// ---------------------------------------------------

		float pillarW = (wallSz.x - holeSize.x) / 2.0f;
		left.scale = vec3(pillarW, wallSz.y, thick);
		left.pos = vec3(-halfW + (pillarW / 2.0f), 0.0f, 0.0f); // 로컬 좌표

		// (2) 오른쪽 기둥 (왼쪽과 대칭)
		right.scale = vec3(pillarW, wallSz.y, thick);
		right.pos = vec3(halfW - (pillarW / 2.0f), 0.0f, 0.0f);

		float headerH = wallSz.y - holeSize.y;
		top.scale = vec3(holeSize.x, headerH, thick);

		float headerY = -halfH + holeSize.y + (headerH / 2.0f);
		top.pos = vec3(0.0f, headerY, 0.0f);
	}

	// 내부 큐브 그리기 헬퍼
	void DrawSubCube(SubWall sw) {
		glPushMatrix();
		glTranslatef(sw.pos.x, sw.pos.y, sw.pos.z); // 로컬 이동
		glScalef(sw.scale.x, sw.scale.y, sw.scale.z);

		float x = 0.5f, y = 0.5f, z = 0.5f;
		glBegin(GL_QUADS);
		glNormal3f(0, 0, 1); glVertex3f(-x, -y, z); glVertex3f(x, -y, z); glVertex3f(x, y, z); glVertex3f(-x, y, z); // 앞
		glNormal3f(0, 0, -1); glVertex3f(-x, -y, -z); glVertex3f(-x, y, -z); glVertex3f(x, y, -z); glVertex3f(x, -y, -z); // 뒤
		glNormal3f(-1, 0, 0); glVertex3f(-x, -y, -z); glVertex3f(-x, -y, z); glVertex3f(-x, y, z); glVertex3f(-x, y, -z); // 좌
		glNormal3f(1, 0, 0); glVertex3f(x, -y, -z); glVertex3f(x, y, -z); glVertex3f(x, y, z); glVertex3f(x, -y, z); // 우
		glNormal3f(0, 1, 0); glVertex3f(-x, y, -z); glVertex3f(-x, y, z); glVertex3f(x, y, z); glVertex3f(x, y, -z); // 위
		glNormal3f(0, -1, 0); glVertex3f(-x, -y, -z); glVertex3f(x, -y, -z); glVertex3f(x, -y, z); glVertex3f(-x, -y, z); // 아래
		glEnd();
		glPopMatrix();
	}

	void Draw() {
		glPushMatrix();
		// 1. 벽 전체의 위치로 이동
		glTranslatef(position.x, position.y, position.z);

		glColor3f(color.r, color.g, color.b);

		// 2. 쪼개진 3조각 그리기
		DrawSubCube(left);
		DrawSubCube(right);
		DrawSubCube(top);

		glPopMatrix();
	}
	// [중요] 충돌 처리를 위해 AABB를 반환할 때 주의!
	// 그냥 GetAABB()를 쓰면 구멍까지 포함된 큰 박스가 되어버려 문을 못 지나감.
};


// -------------------------------------------------------
// [0-1] 전역변수 선언
// -------------------------------------------------------
Door myDoor(vec3(0.0f, 2.0f, -5.0f), vec3(2.6f, 4.0f, 0.0f));          // 우리가 조작할 물체
Cube roomFloor(vec3(0.0f, -0.5f, 0.0f), vec3(40.0f, 1.0f, 40.0f), vec3(0.7f, 0.7f, 0.7f)); // 바닥
Camera mainCamera(vec3(0.0f, 4.0f, 10.0f));

// 벽 공통 색상
vec3 wallColor(0.8f, 0.7f, 0.6f);
float wallThick = 2.0f; // 벽 두께

WallWithHole frontWall( // 앞쪽 벽
	vec3(0.0f, 7.5f, -20.0f - (wallThick / 2)), // 위치
	vec3(40.0f, 15.0f, wallThick),              // 벽 전체 크기
	vec3(2.6f, 4.0f, 0.0f),                     // 구멍 크기 (문 크기)
	wallColor
);
Cube wallBehind(vec3(0.0f, 7.5f, 20.0f + (wallThick / 2)), vec3(40.0f, 15.0f, wallThick), wallColor); // [2] 뒤쪽 벽 (Z = 20)
Cube wallRight(vec3(20.0f + (wallThick / 2), 7.5f, 0.0f), vec3(wallThick, 15.0f, 40.0f), wallColor); // [3] 오른쪽 벽 (X = 20)
Cube wallLeft(vec3(-20.0f - (wallThick / 2), 7.5f, 0.0f), vec3(wallThick, 15.0f, 40.0f), wallColor); // [4] 왼쪽 벽 (X = -20)


bool isHolding = false;     // 잡고 있는지 여부
float grabDistance = 0.0f;    // 잡았을 때의 거리
vec3 grabOriginalScale; // 잡았을 때의 원래 크기



// -------------------------------------------------------
// [2] 로직 함수
// -------------------------------------------------------
void SetupLighting() {
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    GLfloat lightPos[] = { 0.0f, 20.0f, 0.0f, 1.0f };
    GLfloat white[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, white);
    glLightfv(GL_LIGHT0, GL_SPECULAR, white);
}

bool IsLookingAtCube() {
    vec3 toCube = myDoor.position - mainCamera.Pos;
    float dist = length(toCube);
    if (dist > 30.0f) return false;

    toCube = normalize(toCube);
    float angle = dot(mainCamera.Front, toCube);

    return (angle > 0.95f);
}

float GetWallDistance() {
    float minT = 1000.0f;

    struct Plane { vec3 p0; vec3 n; };
    vector<Plane> planes;

    planes.push_back({ vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f) });
    planes.push_back({ vec3(0.0f, 15.0f, 0.0f), vec3(0.0f, -1.0f, 0.0f) });
    planes.push_back({ vec3(0.0f, 0.0f, -20.0f), vec3(0.0f, 0.0f, 1.0f) }); // 앞벽
    planes.push_back({ vec3(0.0f, 0.0f, 20.0f), vec3(0.0f, 0.0f, -1.0f) });
    planes.push_back({ vec3(-20.0f, 0.0f, 0.0f), vec3(1.0f, 0.0f, 0.0f) });
    planes.push_back({ vec3(20.0f, 0.0f, 0.0f), vec3(-1.0f, 0.0f, 0.0f) });

    for (const auto& p : planes) {
        float denom = dot(p.n, mainCamera.Front);
        if (abs(denom) > 0.0001f) {
            float t = dot(p.p0 - mainCamera.Pos, p.n) / denom;
            if (t > 0.1f && t < minT) {
                minT = t;
            }
        }
    }
    return minT - 1.5f;
}

// -------------------------------------------------------
// [3] 그리기 함수들
// -------------------------------------------------------
void DrawCube(float w, float h, float d) {
    float x = w / 2.0f;
    float y = h / 2.0f;
    float z = d / 2.0f;

    glBegin(GL_QUADS);
    glNormal3f(0.0f, 0.0f, 1.0f); glVertex3f(-x, -y, z); glVertex3f(x, -y, z); glVertex3f(x, y, z); glVertex3f(-x, y, z);
    glNormal3f(0.0f, 0.0f, -1.0f); glVertex3f(-x, -y, -z); glVertex3f(-x, y, -z); glVertex3f(x, y, -z); glVertex3f(x, -y, -z);
    glNormal3f(-1.0f, 0.0f, 0.0f); glVertex3f(-x, -y, -z); glVertex3f(-x, -y, z); glVertex3f(-x, y, z); glVertex3f(-x, y, -z);
    glNormal3f(1.0f, 0.0f, 0.0f); glVertex3f(x, -y, -z); glVertex3f(x, y, -z); glVertex3f(x, y, z); glVertex3f(x, -y, z);
    glNormal3f(0.0f, 1.0f, 0.0f); glVertex3f(-x, y, -z); glVertex3f(-x, y, z); glVertex3f(x, y, z); glVertex3f(x, y, -z);
    glNormal3f(0.0f, -1.0f, 0.0f); glVertex3f(-x, -y, -z); glVertex3f(x, -y, -z); glVertex3f(x, -y, z); glVertex3f(-x, -y, z);
    glEnd();
}

//void DrawDesk() {
//    float deskW = 6.0f, deskD = 3.0f, deskH = 2.5f;
//    float legThick = 0.3f, topThick = 0.2f;
//    glColor3f(0.6f, 0.4f, 0.2f);
//
//    float legX = (deskW / 2) - (legThick / 2);
//    float legZ = (deskD / 2) - (legThick / 2);
//    float legY = deskH / 2;
//
//    glPushMatrix(); glTranslatef(-legX, legY, legZ); DrawCube(legThick, deskH, legThick); glPopMatrix();
//    glPushMatrix(); glTranslatef(legX, legY, legZ); DrawCube(legThick, deskH, legThick); glPopMatrix();
//    glPushMatrix(); glTranslatef(-legX, legY, -legZ); DrawCube(legThick, deskH, legThick); glPopMatrix();
//    glPushMatrix(); glTranslatef(legX, legY, -legZ); DrawCube(legThick, deskH, legThick); glPopMatrix();
//
//    glColor3f(0.7f, 0.5f, 0.3f);
//    glPushMatrix();
//    glTranslatef(0.0f, deskH + (topThick / 2), 0.0f);
//    DrawCube(deskW, topThick, deskD);
//    glPopMatrix();
//}


// [방 그리기]
void DrawRoom() {
	// 1. 바닥 그리기 (기존과 동일)
	roomFloor.Draw();
	// 2. 벽 그리기
	// [벽 1: 구멍 뚫린 벽 (Z = -20)] 
	glNormal3f(0, 0, 1);
	frontWall.Draw();

	// [벽 2: 뒤쪽 (Z = 20)]
	// 노멀 방향 반대 (방 안쪽을 향하게)
	glNormal3f(0, 0, -1);
	wallBehind.Draw();

	// [벽 3: 오른쪽 (X = 20)]
	glNormal3f(-1, 0, 0);
	wallRight.Draw();

	// [벽 4: 왼쪽 (X = -20)]
	glNormal3f(1, 0, 0);
	wallLeft.Draw();

	glEnd();
}


void DrawCrosshair() {
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    glOrtho(0, windowWidth, 0, windowHeight, -1, 1);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    glDisable(GL_LIGHTING); glDisable(GL_DEPTH_TEST);

    if (IsLookingAtCube()) glColor3f(1.0f, 0.0f, 0.0f);
    else glColor3f(0.0f, 1.0f, 0.0f);

    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glVertex2f(windowWidth / 2.0f - 10.0f, windowHeight / 2.0f);
    glVertex2f(windowWidth / 2.0f + 10.0f, windowHeight / 2.0f);
    glVertex2f(windowWidth / 2.0f, windowHeight / 2.0f - 10.0f);
    glVertex2f(windowWidth / 2.0f, windowHeight / 2.0f + 10.0f);
    glEnd();

    glEnable(GL_DEPTH_TEST); glEnable(GL_LIGHTING);
    glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(GL_MODELVIEW); glPopMatrix();
}

// [정답 확인 함수]
void CheckAnswer() {
    Door* door = &myDoor;
    vec3 goalPos = vec3(0.0f, 2.0f, -20.0f);
    vec3 goalScale = vec3(2.6f, 4.0f, 0.2f);

    float d = distance(door->position, goalPos);
    float sx = abs(door->scale.x - goalScale.x);
    float sy = abs(door->scale.y - goalScale.y);

    if (d < 4.0f && sx < 1.5f && sy < 1.5f) {
        cout << "탈출 성공!" << endl;
        isLevelClear = true;

        door->position = goalPos;
        door->scale = goalScale;
        door->rotation.y = 85.0f; // 문 열림
        door->position.x -= 1.2f;
        door->position.z -= 1.0f;
    }
}

void MyDisplay() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();

    vec3 target = mainCamera.Pos + mainCamera.Front;
    gluLookAt(mainCamera.Pos.x, mainCamera.Pos.y, mainCamera.Pos.z,
        target.x, target.y, target.z,
        mainCamera.Up.x, mainCamera.Up.y, mainCamera.Up.z);

    DrawRoom();

    glPushMatrix();
    glTranslatef(-5.0f, 0.0f, -5.0f); glRotatef(45.0f, 0.0f, 1.0f, 0.0f);
    //DrawDesk();
    glPopMatrix();

    if (isHolding) {
        float currentDist = GetWallDistance(); // 플레이어 시선이 닿는 벽까지의 거리 계산
        float scaleRatio = currentDist / grabDistance; // 원래 거리 대비 현재 거리 비율
        myDoor.position = mainCamera.Pos + (mainCamera.Front * currentDist);
        myDoor.scale = grabOriginalScale * scaleRatio;
        myDoor.rotation = vec3(0, 0, 0);
    }

    myDoor.Draw();
    DrawCrosshair();
    glutSwapBuffers();
}

void MyReshape(int w, int h) {
    windowWidth = w; windowHeight = h;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    gluPerspective(45.0f, (float)w / h, 0.1f, 100.0f);
}

void MyPassiveMotion(int x, int y) {
    float cx = glutGet(GLUT_WINDOW_WIDTH) / 2;
    float cy = glutGet(GLUT_WINDOW_HEIGHT) / 2;
    float xoffset = x - cx; float yoffset = cy - y;
    if (xoffset == 0 && yoffset == 0) return;
    mainCamera.ProcessMouse(xoffset, yoffset);
    glutWarpPointer(cx, cy);
    glutPostRedisplay();
}

void MyKeyboard(unsigned char key, int x, int y) {
    switch (key) {
    case 'w': mainCamera.ProcessKeyboard(0); break;
    case 's': mainCamera.ProcessKeyboard(1); break;
    case 'a': mainCamera.ProcessKeyboard(2); break;
    case 'd': mainCamera.ProcessKeyboard(3); break;
    case 27: exit(0); break;
    }
    glutPostRedisplay();
}

void MyMouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        if (isHolding) {
            isHolding = false;
            CheckAnswer();
        }
        else {
            if (isLevelClear) return; 
            if (IsLookingAtCube()) {
                isHolding = true;
                grabDistance = distance(mainCamera.Pos, myDoor.position);
                grabOriginalScale = myDoor.scale;
            }
        }
    }
}

void MyTimer(int value) {
    glutPostRedisplay();
    if (!isHolding) {
        // 바닥 높이 (물체 높이의 절반)
        float groundLevel = myDoor.scale.y / 2.0f;

        if (myDoor.position.y > groundLevel) {
            myDoor.position.y -= 0.2f; // 떨어지는 속도
            if (myDoor.position.y < groundLevel) {
                myDoor.position.y = groundLevel;
            }
        }
    }
    glutTimerFunc(16, MyTimer, 1);
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(windowWidth, windowHeight);
    glutCreateWindow("Perspective Game");

    glewInit();
    glEnable(GL_DEPTH_TEST);
    SetupLighting();
    myDoor.LoadTexture(textureFilePath);

    glutSetCursor(GLUT_CURSOR_NONE);
    glutWarpPointer(windowWidth / 2, windowHeight / 2);

    glutDisplayFunc(MyDisplay);
    glutReshapeFunc(MyReshape);
    glutKeyboardFunc(MyKeyboard);
    glutPassiveMotionFunc(MyPassiveMotion);
    glutMouseFunc(MyMouse);
    glutTimerFunc(16, MyTimer, 1);

    glutMainLoop();
    return 0;
}