#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include <csignal>
#include <dirent.h>
#include <unistd.h>

using namespace std;

struct Process {
    int pid;
    string name;
    double cpuUsage;
    double memUsage;
};

// Function to read CPU utilization (overall)
double getCPUUsage() {
    static long prevIdle = 0, prevTotal = 0;
    ifstream file("/proc/stat"); //Linux file containing CPU info
    string line;    // Empty container for line
    getline(file, line);    // Read the first line until newline
    file.close();   // Close the file

    string cpu; // to hold "cpu" label
    long user, nice, system, idle, iowait, irq, softirq, steal; // CPU time components for software analysis for both the modes user and kernel
    istringstream ss(line);// ss is the object,Create a string stream from the line
    ss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
    long idleTime = idle + iowait;
    long totalTime = user + nice + system + idle + iowait + irq + softirq + steal;

    long diffIdle = idleTime - prevIdle;
    long diffTotal = totalTime - prevTotal;
    prevIdle = idleTime;
    prevTotal = totalTime;

    double cpuPercent = (1.0 - (double)diffIdle / diffTotal) * 100.0;
    return cpuPercent;
}

// Function to read Memory utilization (overall)
double getMemoryUsage() {
    ifstream file("/proc/meminfo");
    string label;
    long totalMem = 0, freeMem = 0;
    string unit;
    file >> label >> totalMem >> unit; // MemTotal
    file >> label >> freeMem >> unit;  // MemFree
    file.close();
    return 100.0 * (totalMem - freeMem) / totalMem;
}

// Function to read uptime
double getUptime() {
    ifstream file("/proc/uptime");
    double uptime;
    file >> uptime;
    file.close();
    return uptime / 3600; // in hours
}

// Function to get per-process info
vector<Process> getProcesses() {
    vector<Process> processes;
    DIR* dir = opendir("/proc");
    if (!dir) return processes;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (isdigit(entry->d_name[0])) {
            int pid = stoi(entry->d_name);
            string statPath = "/proc/" + string(entry->d_name) + "/stat";
            string statusPath = "/proc/" + string(entry->d_name) + "/status";

            ifstream statFile(statPath);
            ifstream statusFile(statusPath);
            if (!statFile.is_open() || !statusFile.is_open()) continue;

            string name;
            string comm;
            long utime, stime, starttime;
            double cpuUsage = 0, memUsage = 0;

            string token;
            statFile >> pid >> comm;
            for (int i = 0; i < 11; ++i) statFile >> token;
            statFile >> utime >> stime;
            for (int i = 0; i < 7; ++i) statFile >> token;
            statFile >> starttime;

            long total_time = utime + stime;
            long seconds = sysconf(_SC_CLK_TCK);
            cpuUsage = (double)total_time / seconds;

            string line;
            long vmrss = 0;
            while (getline(statusFile, line)) {
                if (line.find("VmRSS:") == 0) {
                    istringstream ss(line);
                    string label, unit;
                    ss >> label >> vmrss >> unit;
                    memUsage = vmrss / 1024.0; // MB
                    break;
                }
            }

            statFile.close();
            statusFile.close();

            // Clean process name
            comm.erase(remove(comm.begin(), comm.end(), '('), comm.end());
            comm.erase(remove(comm.begin(), comm.end(), ')'), comm.end());

            processes.push_back({pid, comm, cpuUsage, memUsage});
        }
    }
    closedir(dir);
    return processes;
}

// Function to display processes
void displayProcesses(vector<Process>& processes) {
    cout << left << setw(8) << "PID" 
         << setw(25) << "Process Name" 
         << setw(12) << "CPU (s)" 
         << setw(12) << "MEM (MB)" << endl;
    cout << string(60, '-') << endl;
    for (int i = 0; i < min((int)processes.size(), 10); ++i) {
        cout << left << setw(8) << processes[i].pid
             << setw(25) << processes[i].name
             << setw(12) << fixed << setprecision(2) << processes[i].cpuUsage
             << setw(12) << processes[i].memUsage
             << endl;
    }
}

// Function to kill a process
void killProcess(int pid) {
    if (kill(pid, SIGTERM) == 0)
        cout << "Process " << pid << " terminated successfully.\n";
    else
        perror("Error terminating process");
}

// Main function
int main() {
    while (true) {
        system("clear");

        cout << "==================== SYSTEM MONITOR ====================" << endl;
        cout << "CPU Usage: " << fixed << setprecision(2) << getCPUUsage() << "%" << endl;
        cout << "Memory Usage: " << fixed << setprecision(2) << getMemoryUsage() << "%" << endl;
        cout << "System Uptime: " << fixed << setprecision(2) << getUptime() << " hours" << endl;
        cout << "========================================================" << endl;

        vector<Process> processes = getProcesses();

        cout << "\nSort by: 1) CPU  2) Memory  (default: CPU): ";
        int choice;
        if (!(cin >> choice)) { cin.clear(); cin.ignore(10000, '\n'); choice = 1; }

        if (choice == 2)
            sort(processes.begin(), processes.end(), [](const Process& a, const Process& b) {
                return a.memUsage > b.memUsage;
            });
        else
            sort(processes.begin(), processes.end(), [](const Process& a, const Process& b) {
                return a.cpuUsage > b.cpuUsage;
            });

        displayProcesses(processes);

        cout << "\nEnter PID to kill or 0 to refresh: ";
        int pid;
        cin >> pid;
        if (pid > 0) killProcess(pid);

        this_thread::sleep_for(chrono::seconds(3));
    }
    return 0;
}