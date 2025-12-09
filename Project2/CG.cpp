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
const char* textureFilePath = "Data/wood.bmp";

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

// 플레이어 벽 충돌 체크 함수

vec3 CheckPlayerCollision(vec3 currentPos, vec3 nextPos) {
    // 1. 전체 맵의 외곽 경계 (Room 1 + Room 2)
    // Z축: Room 1 뒤쪽(19.5) ~ Room 2 끝쪽(-59.5)
    if (nextPos.z > 19.5f) nextPos.z = 19.5f;
    if (nextPos.z < -59.5f) nextPos.z = -59.5f;

    // X축: 좌우 폭 (-19.5 ~ 19.5)
    if (nextPos.x > 19.5f) nextPos.x = 19.5f;
    if (nextPos.x < -19.5f) nextPos.x = -19.5f;

    // 바닥 높이
    if (nextPos.y < 2.0f) nextPos.y = 2.0f;

    // 2. 중간 벽 (Z = -20) 충돌 처리
    // 벽은 -20 위치에 있고 두께가 있으므로, -19.5 ~ -20.5 구간이 '벽 내부'입니다.
    bool insideWallZone = (nextPos.z < -19.5f && nextPos.z > -20.5f);

    if (insideWallZone) {
        // [통과 조건] 레벨이 클리어되었고 && 구멍 위치(X 중앙)여야 함
        bool isAtHole = (abs(nextPos.x) < 1.3f);

        if (isLevelClear && isAtHole) {
            // 통과 허용 (아무것도 안 함, nextPos 유지)
        }
        else {
            // [충돌 발생!] 
            // 중요: 플레이어가 '어디서' 왔는지에 따라 밀어내는 방향을 다르게 함

            if (currentPos.z > -20.0f) {
                // 방 1(앞쪽)에 있었으면 -> 방 1 쪽으로 밀어냄
                nextPos.z = -19.5f;
            }
            else {
                // 방 2(뒤쪽)에 있었으면 -> 방 2 쪽으로 밀어냄
                nextPos.z = -20.5f;
            }
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

class AnamorphicPuzzle {
public:
    struct Piece {
        vec3 pos;
        vec3 scale;
        vec3 rot;
    };
    vector<Piece> pieces;

    vec3 projectorPos; // 정답을 볼 수 있는 위치 (플레이어가 서야 할 곳)
    vec3 lookAtTarget; // 시선이 향하는 곳 (구조물 중심)
    GLuint texID;

    // 생성자: 큐브들을 랜덤하게 배치
    AnamorphicPuzzle() {
        // [수정됨] 정답 위치 (프로젝터)
        // 기존 Z: -25.0f -> -32.0f (타겟에 더 가까워짐)
        // 기존 X: 15.0f -> 12.0f (각도를 약간 완만하게)
        projectorPos = vec3(12.0f, 4.0f, -32.0f);

        // 타겟 위치 (방 2의 안쪽 중앙)
        lookAtTarget = vec3(0.0f, 5.0f, -50.0f);

        texID = 0;
    }

    // 초기화: 큐브 랜덤 생성 및 텍스처 로드
    // 최적화 버전: 50개 + 적당한 크기
// [수정됨] 넓게 퍼뜨리고, 작게 만들어서 '파편 아트' 느낌 내기
    // [수정됨] 사용자 설정 변수(Tuning Variables) 추가
    void Init(const char* texturePath) {
        // --------------------------------------------------------
        // [여기를 조절하세요!] 퍼짐 정도와 크기 조절 변수
        // --------------------------------------------------------
        float spreadX = 13.0f;   // 좌우로 얼마나 퍼질지 (클수록 넓게 퍼짐)
        float spreadY = 9.0f;   // 위아래로 얼마나 퍼질지
        float spreadZ = 13.0f;  // 앞뒤 깊이감 (클수록 옆에서 봤을 때 더 깨져 보임)

        float centerY = 5.0f;   // 큐브들이 모일 중심 높이
        float centerZ = -45.0f; // 큐브들이 모일 중심 깊이 (방 안쪽)

        float minSize = 1.0f;   // 큐브 최소 크기
        float maxSize = 2.0f;   // 큐브 최대 크기 (최소와 차이가 커야 다양해 보임)
        // --------------------------------------------------------

        // 텍스처 로드 (기존과 동일)
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

        // 60개 생성
        for (int i = 0; i < 100; i++) {
            Piece p;

            // 난수 생성 (0.0 ~ 1.0)
            float r1 = (rand() % 1000) / 1000.0f;
            float r2 = (rand() % 1000) / 1000.0f;
            float r3 = (rand() % 1000) / 1000.0f;
            float r4 = (rand() % 1000) / 1000.0f;

            // [위치] 설정한 spread 변수를 곱해서 범위 조절
            // 예: spreadX가 10이면, -5 ~ +5 범위에 배치
            float rx = (r1 * spreadX) - (spreadX / 2.0f);
            float ry = (r2 * spreadY) - (spreadY / 2.0f) + centerY;
            float rz = (r3 * spreadZ) - (spreadZ / 2.0f) + centerZ;

            p.pos = vec3(rx, ry, rz);

            // [크기] minSize ~ maxSize 사이에서 랜덤 결정
            float scale = minSize + (r4 * (maxSize - minSize));
            p.scale = vec3(scale, scale, scale);

            // 회전
            p.rot = vec3(rand() % 360, rand() % 360, rand() % 360);

            pieces.push_back(p);
        }
    }

    // [핵심] 월드 좌표(v)를 정답 시점의 UV 좌표로 변환하는 함수
    vec2 GetProjectedUV(vec3 worldPos) {
        // 1. View Matrix (정답 위치에서 타겟을 바라보는 행렬)
        mat4 view = lookAt(projectorPos, lookAtTarget, vec3(0, 1, 0));

        // 2. Projection Matrix (카메라와 동일한 45도 화각)
        mat4 proj = perspective(radians(45.0f), (float)800 / 600, 0.1f, 100.0f);

        // 3. World -> Clip Space 변환
        vec4 clipSpace = proj * view * vec4(worldPos, 1.0f);

        // 4. Perspective Divide (NDC 변환)
        vec3 ndc = vec3(clipSpace) / clipSpace.w;

        // 5. NDC(-1~1) -> UV(0~1) 변환
        return vec2(ndc.x * 0.5f + 0.5f, ndc.y * 0.5f + 0.5f);
    }

    void Draw() {
        if (texID == 0) return;

        // [핵심 변경] 조명 끄기 (Unlit)
        // 조명을 끄면 그림자가 사라져서 완벽한 평면 이미지처럼 보임
        glDisable(GL_LIGHTING);

        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, texID);

        // 텍스처 색상을 그대로 출력 (환경광 무시)
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

        for (const auto& p : pieces) {
            glPushMatrix();
            glTranslatef(p.pos.x, p.pos.y, p.pos.z);
            glRotatef(p.rot.x, 1, 0, 0);
            glRotatef(p.rot.y, 0, 1, 0);
            glRotatef(p.rot.z, 0, 0, 1);

            float s = p.scale.x / 2.0f;
            vec3 v[8] = {
                {-s,-s, s}, { s,-s, s}, { s, s, s}, {-s, s, s},
                {-s,-s,-s}, { s,-s,-s}, { s, s,-s}, {-s, s,-s}
            };

            mat4 model = mat4(1.0f);
            model = translate(model, p.pos);
            model = rotate(model, radians(p.rot.x), vec3(1, 0, 0));
            model = rotate(model, radians(p.rot.y), vec3(0, 1, 0));
            model = rotate(model, radians(p.rot.z), vec3(0, 0, 1));

            int faces[6][4] = {
                {0, 1, 2, 3}, {5, 4, 7, 6}, {4, 0, 3, 7},
                {1, 5, 6, 2}, {3, 2, 6, 7}, {4, 5, 1, 0}
            };

            // 법선(Normal)은 조명 계산용이라 조명을 끄면 필요 없지만, 
            // 혹시 모르니 남겨둡니다.
            vec3 normals[6] = {
                {0,0,1}, {0,0,-1}, {-1,0,0}, {1,0,0}, {0,1,0}, {0,-1,0}
            };

            glBegin(GL_QUADS);
            for (int i = 0; i < 6; i++) {
                // glNormal3fv(value_ptr(normals[i])); // 조명 껐으니 주석 처리해도 됨

                for (int j = 0; j < 4; j++) {
                    vec3 localPos = v[faces[i][j]];
                    vec4 worldPos4 = model * vec4(localPos, 1.0f);
                    vec3 worldPos = vec3(worldPos4);

                    vec2 uv = GetProjectedUV(worldPos);

                    glTexCoord2f(uv.x, uv.y);
                    glVertex3f(localPos.x, localPos.y, localPos.z);
                }
            }
            glEnd();

            glPopMatrix();
        }

        // 뒷정리: 다른 물체들을 위해 설정을 원상복구
        glDisable(GL_TEXTURE_2D);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE); // 기본 모드로 복구
        glEnable(GL_LIGHTING); // 조명 다시 켜기
    }

    // 플레이어가 정답 위치에 있는지 검사
    bool CheckSolved(vec3 playerPos) {
        float dist = distance(playerPos, projectorPos);
        // 정답 위치 반경 2.0 안에 들어오면 성공
        return (dist < 2.0f);
    }
};


// -------------------------------------------------------
// [0-1] 전역변수 선언
// -------------------------------------------------------
Door myDoor(vec3(0.0f, 2.0f, -5.0f), vec3(2.6f, 4.0f, 0.0f));          // 우리가 조작할 물체
Cube roomFloor(vec3(0.0f, -0.5f, 0.0f), vec3(40.0f, 1.0f, 40.0f), vec3(0.7f, 0.7f, 0.7f)); // 바닥
Camera mainCamera(vec3(0.0f, 4.0f, 10.0f));
AnamorphicPuzzle myPuzzle;
bool isPuzzleClear = false; // 2번방 퍼즐 풀었는지 여부
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
// [기존 변수들 아래에 추가] Room 2 (두 번째 방) 구조물 정의
Cube room2Floor(vec3(0.0f, -0.5f, -40.0f), vec3(40.0f, 1.0f, 40.0f), vec3(0.6f, 0.6f, 0.6f)); // 방2 바닥 (약간 어두운 색)
Cube room2Back(vec3(0.0f, 7.5f, -60.0f - (wallThick / 2)), vec3(40.0f, 15.0f, wallThick), wallColor); // 방2 뒷벽
Cube room2Right(vec3(20.0f + (wallThick / 2), 7.5f, -40.0f), vec3(wallThick, 15.0f, 40.0f), wallColor); // 방2 오른쪽 벽
Cube room2Left(vec3(-20.0f - (wallThick / 2), 7.5f, -40.0f), vec3(wallThick, 15.0f, 40.0f), wallColor); // 방2 왼쪽 벽


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

    room2Floor.Draw();      // 방2 바닥
    glNormal3f(0, 0, 1); 
    room2Back.Draw();  // 방2 뒷벽

    glNormal3f(-1, 0, 0);
    room2Right.Draw(); // 방2 오른쪽 벽

    glNormal3f(1, 0, 0); 
    room2Left.Draw();  // 방2 왼쪽 벽


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
    myPuzzle.Draw();

    // [추가] 정답 위치 힌트 (어디 서야할지 모르니 빨간 공 하나 띄워줌)
    if (!isPuzzleClear) {
        glPushMatrix();
        glTranslatef(myPuzzle.projectorPos.x, myPuzzle.projectorPos.y, myPuzzle.projectorPos.z);
        glColor3f(1.0f, 0.0f, 0.0f); // 빨간색
        glutWireSphere(0.3f, 10, 10); // 여기가 정답 위치다! 표시
        glPopMatrix();
    }
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
    //2번방퍼즐
    if (!isPuzzleClear && myPuzzle.CheckSolved(mainCamera.Pos)) {
        cout << "그림 완성! 퍼즐 해결!" << endl;
        isPuzzleClear = true;
    }
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
    myPuzzle.Init(textureFilePath);
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