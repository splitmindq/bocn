#include <iostream>
#include <string>
#include <vector>
#include <windows.h>
#include <limits>

void print_last_error();
HANDLE open_com_port(int portNum);
void configure_com_port(HANDLE hComm, DWORD baudRate);
void send_string(HANDLE hComm, const std::string& data);
void receive_and_print_string(HANDLE hComm);
void clear_com_buffer(HANDLE hComm);
DWORD select_baud_rate();
std::vector<std::pair<int, int>> findComPortPairs();

HANDLE hComm1 = NULL;
HANDLE hComm2 = NULL;

int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    auto portPairs = findComPortPairs();
    if (portPairs.empty()) {
        std::cerr << "Пары COM-портов не найдены (например, COM1<->COM2)." << std::endl;
        return 1;
    }

    std::cout << "Доступные пары COM-портов:" << std::endl;
    for (const auto& pair : portPairs) {
        std::cout << "  COM" << pair.first << " <-> COM" << pair.second << std::endl;
    }

    int writePort;
    while (true) {
        std::cout << "Введите номер COM-порта для отправки (например, 2 для COM2): ";
        std::string input;
        std::getline(std::cin, input);
        try {
            writePort = std::stoi(input);
            bool valid = false;
            for (const auto& pair : portPairs) {
                if (pair.first == writePort || pair.second == writePort) {
                    valid = true;
                    break;
                }
            }
            if (valid) break;
            std::cerr << "Недопустимый порт. Выберите из списка пар." << std::endl;
        } catch (...) {
            std::cerr << "Некорректный ввод. Введите число." << std::endl;
        }
    }

    int readPort = -1;
    for (const auto& pair : portPairs) {
        if (pair.first == writePort) {
            readPort = pair.second;
            break;
        } else if (pair.second == writePort) {
            readPort = pair.first;
            break;
        }
    }

    hComm1 = open_com_port(writePort);
    hComm2 = open_com_port(readPort);
    if (hComm1 == NULL || hComm2 == NULL) {
        if (hComm1 != NULL) CloseHandle(hComm1);
        if (hComm2 != NULL) CloseHandle(hComm2);
        std::cerr << "Не удалось открыть один или оба COM-порта. Проверьте их доступность." << std::endl;
        return 1;
    }

    DWORD baudRate = select_baud_rate();
    configure_com_port(hComm1, baudRate);
    configure_com_port(hComm2, baudRate);

    std::string message;
    int choice = -1;

    system("cls");
    std::cout << "Отправка через COM" << writePort << ", чтение с COM" << readPort << " на скорости " << baudRate << " бод." << std::endl;

    while (true) {
        std::cout << "\nВыберите действие:\n";
        std::cout << "1 - Отправить сообщение с COM" << writePort << " на COM" << readPort << "\n";
        std::cout << "2 - Прочитать сообщение с COM" << readPort << "\n";
        std::cout << "3 - Изменить скорость передачи\n";
        std::cout << "0 - Выход\n";
        std::cout << "Ваш выбор: ";
        std::cin >> choice;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        switch (choice) {
            case 1:
                std::cout << "Введите сообщение для отправки: ";
                std::getline(std::cin, message);
                clear_com_buffer(hComm2);
                send_string(hComm1, message);
                break;
            case 2:
                receive_and_print_string(hComm2);
                break;
            case 3:
                baudRate = select_baud_rate();
                configure_com_port(hComm1, baudRate);
                configure_com_port(hComm2, baudRate);
                std::cout << "Скорость изменена на " << baudRate << " бод." << std::endl;
                break;
            case 0:
                std::cout << "Выход из программы.\n";
                CloseHandle(hComm1);
                CloseHandle(hComm2);
                return 0;
            default:
                std::cout << "Неверный выбор. Попробуйте снова.\n";
                break;
        }
    }
}

void print_last_error() {
    DWORD dwError = GetLastError();
    if (dwError != 0) {
        LPVOID messageBuffer;
        FormatMessageA(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL,
                dwError,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPSTR)&messageBuffer,
                0,
                NULL
        );
        std::cerr << "Ошибка: " << (LPSTR)messageBuffer << " (Код: " << dwError << ")" << std::endl;
        LocalFree(messageBuffer);
    }
}

HANDLE open_com_port(int portNum) {
    std::wstring portName = L"\\\\.\\" + std::wstring(L"COM") + std::to_wstring(portNum);
    HANDLE hComm = CreateFileW(
            portName.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            0,
            NULL
    );

    if (hComm == INVALID_HANDLE_VALUE) {
        std::cerr << "Не удалось открыть COM" << portNum << std::endl;
        print_last_error();
        return NULL;
    }

    std::cout << "COM" << portNum << " успешно открыт." << std::endl;
    return hComm;
}

void configure_com_port(HANDLE hComm, DWORD baudRate) {
    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

    if (!GetCommState(hComm, &dcbSerialParams)) {
        std::cerr << "Ошибка получения параметров порта." << std::endl;
        print_last_error();
        return;
    }

    dcbSerialParams.BaudRate = baudRate;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;

    if (!SetCommState(hComm, &dcbSerialParams)) {
        std::cerr << "Ошибка настройки параметров порта." << std::endl;
        print_last_error();
        return;
    }

    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;

    if (!SetCommTimeouts(hComm, &timeouts)) {
        std::cerr << "Ошибка настройки тайм-аутов порта." << std::endl;
        print_last_error();
        return;
    }
    std::cout << "Порт настроен. Скорость: " << baudRate << std::endl;
}

void send_string(HANDLE hComm, const std::string& data) {
    DWORD bytesWritten;
    std::string data_with_crlf = data + "\r\n";
    if (WriteFile(hComm, data_with_crlf.c_str(), data_with_crlf.size(), &bytesWritten, NULL)) {
//        std::cout << "Отправлено: " << data_with_crlf << "(Байт: " << bytesWritten << ")" << std::flush << std::endl;
    } else {
        std::cerr << "Ошибка отправки сообщения." << std::endl;
        print_last_error();
    }
}

void receive_and_print_string(HANDLE hComm) {
    DWORD bytesRead;
    char buffer[256] = {0};
    std::string result;
    COMSTAT com_stat;
    DWORD dw_error;
    if (!ClearCommError(hComm, &dw_error, &com_stat)) {
        std::cerr << "Ошибка ClearCommError" << std::endl;
        print_last_error();
        return;
    }

    if (com_stat.cbInQue == 0) {
        std::cerr << "Нет данных для чтения" << std::endl;
        Sleep(2000);
        system("cls");
        return;
    }

    while (true) {
        if (ReadFile(hComm, buffer, sizeof(buffer)-1, &bytesRead, NULL)) {
            if (bytesRead > 0) {
                buffer[bytesRead] = '\0';
                result += buffer;
                if (result.find("\r\n") != std::string::npos) break;
            } else {
                std::cerr << "Ошибка чтения сообщения." << std::endl;
                print_last_error();            }
        }
    }
    std::cout << result << std::endl;

}

DWORD select_baud_rate() {
    std::vector<DWORD> baudRates = {300, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200};
    std::cout << "Доступные скорости передачи:" << std::endl;
    for (size_t i = 0; i < baudRates.size(); ++i) {
        std::cout << i + 1 << " - " << baudRates[i] << std::endl;
    }
    std::cout << "Введите номер: ";

    int choice;
    std::cin >> choice;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    if (choice >= 1 && choice <= static_cast<int>(baudRates.size())) {
        std::cout << "Выбрана скорость: " << baudRates[choice - 1] << std::endl;
        return baudRates[choice - 1];
    } else {
        std::cout << "Неверный выбор. Устанавливается скорость по умолчанию: 9600." << std::endl;
        return 9600;
    }
}

std::vector<std::pair<int, int>> findComPortPairs() {
    std::vector<int> ports;
    for (int i = 1; i <= 256; i++) {
        std::wstring portName = L"\\\\.\\" + std::wstring(L"COM") + std::to_wstring(i);
        HANDLE hPort = CreateFileW(
                portName.c_str(),
                GENERIC_READ | GENERIC_WRITE,
                0,
                nullptr,
                OPEN_EXISTING,
                0,
                nullptr
        );
        if (hPort != INVALID_HANDLE_VALUE) {
            ports.push_back(i);
            CloseHandle(hPort);
        }
    }

    std::vector<std::pair<int, int>> portPairs;
    for (size_t i = 0; i < ports.size(); i += 2) {
        if (i + 1 < ports.size()) {
            portPairs.emplace_back(ports[i], ports[i + 1]);
        }
    }
    return portPairs;
}

void clear_com_buffer(HANDLE hComm) {
    if (!PurgeComm(hComm, PURGE_RXCLEAR | PURGE_TXCLEAR)) {
        std::cout << "Ошибка очистки буфера порта." << std::endl;
        print_last_error();
    }
}