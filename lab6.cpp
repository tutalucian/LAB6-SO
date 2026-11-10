#include <iostream>
#include <random>
#include <string>
#include <cstring>
#include <ctime>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
    #include <sys/mman.h>
    #include <sys/wait.h>
    #include <semaphore.h>
#endif

const int LIMITA = 1000;

struct MemoriePartajata {
#ifndef _WIN32
    sem_t semafor;
#endif
    int numar;
    int scrisDe[LIMITA + 1];
};

#ifdef _WIN32
typedef HANDLE Semafor;
void ocupa(Semafor s) { WaitForSingleObject(s, INFINITE); }
void elibereaza(Semafor s) { ReleaseSemaphore(s, 1, NULL); }
void pauza() { Sleep(1); }
unsigned long pidCurent() { return GetCurrentProcessId(); }
#else
typedef sem_t* Semafor;
void ocupa(Semafor s) { sem_wait(s); }
void elibereaza(Semafor s) { sem_post(s); }
void pauza() { usleep(1000); }
unsigned long pidCurent() { return (unsigned long)getpid(); }
#endif

int numara(MemoriePartajata* mem, Semafor semafor, int idProces)
{
    std::mt19937 generator((unsigned)(pidCurent() * 7919u + (unsigned)time(NULL)));
    std::uniform_int_distribution<int> ban(1, 2);
    int scrise = 0;

    while (true) {
        ocupa(semafor);

        int n = mem->numar;
        if (n >= LIMITA) {
            elibereaza(semafor);
            break;
        }

        while (n < LIMITA && ban(generator) == 2) {
            n++;
            mem->numar = n;
            mem->scrisDe[n] = idProces;
            scrise++;
            std::cout << "Procesul " << idProces << " (PID " << pidCurent()
                      << ") a scris " << n << std::endl;
        }

        elibereaza(semafor);
        pauza();
    }
    return scrise;
}

void verifica(MemoriePartajata* mem)
{
    int deLa1 = 0, deLa2 = 0, lipsa = 0;
    for (int i = 1; i <= LIMITA; i++) {
        if (mem->scrisDe[i] == 1) deLa1++;
        else if (mem->scrisDe[i] == 2) deLa2++;
        else lipsa++;
    }

    std::cout << std::endl << "Numarul final din memorie: " << mem->numar << std::endl;
    std::cout << "Procesul 1 a scris " << deLa1 << " numere" << std::endl;
    std::cout << "Procesul 2 a scris " << deLa2 << " numere" << std::endl;
    if (lipsa == 0 && deLa1 + deLa2 == LIMITA)
        std::cout << "Toate numerele de la 1 la " << LIMITA
                  << " au fost scrise o singura data, in ordine." << std::endl;
    else
        std::cout << "Lipsesc " << lipsa << " numere!" << std::endl;
}

#ifdef _WIN32

const char* NUME_MEMORIE = "Local\\Lab6Memorie";
const char* NUME_SEMAFOR = "Local\\Lab6Semafor";

int main(int argc, char* argv[])
{
    if (argc > 1 && strcmp(argv[1], "copil") == 0) {
        HANDLE hMap = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, NUME_MEMORIE);
        HANDLE semafor = OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, FALSE, NUME_SEMAFOR);
        if (hMap == NULL || semafor == NULL) {
            std::cerr << "Copilul nu a putut deschide memoria sau semaforul: "
                      << GetLastError() << std::endl;
            return 1;
        }
        MemoriePartajata* mem = (MemoriePartajata*)MapViewOfFile(
            hMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(MemoriePartajata));

        numara(mem, semafor, 2);

        UnmapViewOfFile(mem);
        CloseHandle(hMap);
        CloseHandle(semafor);
        return 0;
    }

    HANDLE hMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                     0, sizeof(MemoriePartajata), NUME_MEMORIE);
    if (hMap == NULL) {
        std::cerr << "Eroare la CreateFileMapping: " << GetLastError() << std::endl;
        return 1;
    }
    MemoriePartajata* mem = (MemoriePartajata*)MapViewOfFile(
        hMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(MemoriePartajata));
    if (mem == NULL) {
        std::cerr << "Eroare la MapViewOfFile: " << GetLastError() << std::endl;
        return 1;
    }
    ZeroMemory(mem, sizeof(MemoriePartajata));

    HANDLE semafor = CreateSemaphoreA(NULL, 1, 1, NUME_SEMAFOR);
    if (semafor == NULL) {
        std::cerr << "Eroare la CreateSemaphore: " << GetLastError() << std::endl;
        return 1;
    }

    std::cout << "Procesul 1 (PID " << GetCurrentProcessId()
              << ") a creat memoria partajata si semaforul." << std::endl << std::endl;

    char caleExe[MAX_PATH];
    GetModuleFileNameA(NULL, caleExe, MAX_PATH);
    std::string comanda = "\"" + std::string(caleExe) + "\" copil";

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessA(NULL, &comanda[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        std::cerr << "Eroare la CreateProcess: " << GetLastError() << std::endl;
        return 1;
    }

    numara(mem, semafor, 1);

    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    verifica(mem);

    UnmapViewOfFile(mem);
    CloseHandle(hMap);
    CloseHandle(semafor);
    return 0;
}

#else

int main()
{
    MemoriePartajata* mem = (MemoriePartajata*)mmap(NULL, sizeof(MemoriePartajata),
        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) {
        std::cerr << "Eroare la mmap()" << std::endl;
        return 1;
    }
    memset(mem, 0, sizeof(MemoriePartajata));

    if (sem_init(&mem->semafor, 1, 1) < 0) {
        std::cerr << "Eroare la sem_init()" << std::endl;
        return 1;
    }

    std::cout << "Procesul 1 (PID " << getpid()
              << ") a creat memoria partajata si semaforul." << std::endl << std::endl;

    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "Eroare la fork()" << std::endl;
        return 1;
    }

    if (pid == 0) {
        numara(mem, &mem->semafor, 2);
        _exit(0);
    }

    numara(mem, &mem->semafor, 1);

    waitpid(pid, NULL, 0);

    verifica(mem);

    sem_destroy(&mem->semafor);
    munmap(mem, sizeof(MemoriePartajata));
    return 0;
}

#endif
