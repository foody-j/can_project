// main.cpp
#include "can_test/motor_can_driver.hpp" //CAN 통신 클래스 정의된 헤더
#include "can_test/motor_data.hpp"
#include <thread>   // sleep_for 사용을 위한 헤더
#include <chrono>   // 시간 관련 기능
#include <signal.h> // SIGINT(Ctrl+C) 처리
#include <iomanip>  //16진수 출력 포맷팅
#include <termios.h>  // 키보드 입력을 위한 헤더 추가
#include <fcntl.h>    // non-blocking 입력을 위한 헤더 추가

// 프로그램 실행 상태 제어를 위한 전역 변수
volatile bool running = true;   //volatile: 최적화 방지, 항상 메모리에서 값 읽음

// Ctrl+C 시그널 핸들러 함수
void signalHandler(int signum) {
    std::cout << "\nCaught signal " << signum << " (Ctrl+C). Terminating...\n";
    running = false;    // 프로그램 종료 플래그 설정
}
// 키보드 입력을 non-blocking으로 설정하는 함수
void setNonBlockingInput() {
    struct termios ttystate;
    tcgetattr(STDIN_FILENO, &ttystate);
    ttystate.c_lflag &= ~(ICANON | ECHO);
    ttystate.c_cc[VMIN] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
}

void parseMotorData(const can_frame& frame, MotorData& data) {
    if (frame.can_dlc >= 8) {
        // Position: Data[0-1], scale 0.1
        int16_t pos_int = (frame.data[0] << 8) | frame.data[1];
        data.position = static_cast<float>(pos_int) * 0.1f;
        
        // Speed: Data[2-3], scale 0.1
        int16_t spd_int = (frame.data[2] << 8) | frame.data[3];
        data.speed = static_cast<float>(spd_int) * 0.1f;
        
        // Current: Data[4-5], scale 0.01
        int16_t cur_int = (frame.data[4] << 8) | frame.data[5];
        data.current = static_cast<float>(cur_int) * 0.01f;
        
        // Temperature: Data[6], direct value
        data.temperature = static_cast<int8_t>(frame.data[6]);
        
        // Error code: Data[7], direct value
        data.error = frame.data[7];
    }
}

void printMotorData(const MotorData& data) {
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "Position: " << data.position << "° "
              << "Speed: " << data.speed << "RPM "
              << "Current: " << data.current << "A "
              << "Temp: " << static_cast<int>(data.temperature) << "°C "
              << "Error: 0x" << std::hex << static_cast<int>(data.error) << std::dec
              << std::endl;
}

int main() {
    signal(SIGINT, signalHandler);  // Ctrl+C 처리
    
    try {
        // 전역 또는 클래스 멤버 변수로 선언
        auto last_time = std::chrono::steady_clock::now();
        int frame_count = 0;
        // CAN 드라이버 초기화 및 연결
        CanComms can_driver;
        std::cout << "Connecting to CAN bus...\n";
        
        // connect 함수 호출 전 딜레이 추가
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        can_driver.connect("can0", 1000000);  // 1Mbps 속도로 연결
        
        // 연결 상태 확인
        if (!can_driver.connected()) {
            std::cerr << "Failed to connect to CAN bus\n";
            return 1;
        }
        
        std::cout << "Successfully connected to CAN bus\n";
        
        // while문 들어가기 전에 딜레이 추가
        std::this_thread::sleep_for(std::chrono::seconds(1));

        while(running) { 
            // CAN 프레임 읽기 및 처리
            struct can_frame frame;
            if (can_driver.readCanFrame(frame)) {
                std::cout << "CAN ID: " << std::hex << frame.can_id << "  ";
                for(int i = 0; i < 8; i++) {
                    // unsigned int로 캐스팅하여 숫자값으로 출력
                    std::cout << static_cast<unsigned int>(frame.data[i]) << " ";
                }
                std::cout << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            
        }
        // 안전한 종료 처리
        std::cout << "\nStopping motor...\n";
        can_driver.write_velocity(1, 0.0f);
        can_driver.disconnect();
        std::cout << "Program terminated successfully\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}