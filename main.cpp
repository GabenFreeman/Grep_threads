#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <string.h>
#include <typeinfo>
#include <unistd.h>
#include <chrono>
#include <ctime>
#include <vector>
#include <thread>
#include <mutex>
#include <queue>

using namespace std;
namespace fs = std::filesystem;

queue<fs::path> file_queue;
mutex file_queue_mutex;
string pattern;
mutex output_mutex;

//resultFile
ofstream resultFile;
string resultFileName ;

//logFile
ofstream logFile;
string logFileName ;

int files = 0;
int nfiles = 0;
int patterns = 0;

void search_file(const fs::path& path)
{
    ifstream file(path);
    bool check=true;
    files++;
    string onlyFile=path;
    if (file.is_open()) {
        string line;
        int coutnLine = 0;
        while (getline(file, line)) {
        coutnLine++;
            if (line.find(pattern) != string::npos) {
                output_mutex.lock();
                resultFile << path << ":" << coutnLine << ": " << line << '\n';
                output_mutex.unlock();
                patterns++;
                if (check){
		        check=false;
		        nfiles++;
		        logFile <<this_thread::get_id()<<":"<<fs::path(onlyFile).filename()<< endl;
                }
            }
        }
    }
}

void search_files()
{
    while (true) {
        file_queue_mutex.lock();
        if (file_queue.empty()) {
            file_queue_mutex.unlock();
            return;
        }
        fs::path path = file_queue.front();
        file_queue.pop();
        file_queue_mutex.unlock();
        if (fs::is_directory(path)) {
            for (const auto& entry : fs::directory_iterator(path)) {
                if (fs::is_directory(entry.path())) {
                    file_queue_mutex.lock();
                    file_queue.push(entry.path());
                    file_queue_mutex.unlock();
                } else {
                    search_file(entry.path());
                }
            }
        } else {
            search_file(path);
        }
    }
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <pattern> <flags>\n"
        "The program has one obligatory parameter which is <pattern (a string)>\n"
        "and can have four optional parameters.\n"
        "This program searches for a specified pattern in files within a specified\n"
        "directory and its subdirectories, and outputs the matches to a result file.\n\n"
        "+----------------------+--------------------------------+--------------------+\n"
        "|       Parameter      |          Description           |   Default value    |\n"
        "+----------------------+--------------------------------+--------------------+\n"
        "| -d or --dir          | start directory where program  |                    |\n"
        "|                      | needs to look for files        | current directory  |\n"
        "|                      | (also in subfolders)           |                    |\n"
        "+----------------------+--------------------------------+--------------------+\n"
        "| -l or --log_file     |     name of the log file       | <program name>.log |\n"
        "+----------------------+--------------------------------+--------------------+\n"
        "| -r or --result_file  |     name of the file where     | <program name>.txt |\n"
        "|                      |        result is given         |                    |\n"
        "+----------------------+--------------------------------+--------------------+\n"
        "| -t or --threads      |  number of threads in the pool |         4          |\n"
        "|                      |                                |                    |\n"
        "+----------------------+--------------------------------+--------------------+\n";
        return 1;
    }
    //set default number of threads
   int num_threads = 4; //threads

    //set default Current working directory
    char dir[256];
    getcwd(dir, 256);
    fs::path directory_path = dir;

    //set pattern
    pattern = argv[1];


    //set default result file
    string txt=".txt";
    resultFileName = argv[0]+txt;
    resultFileName.erase(0, 2);

    //set default log file
    string log=".log";
    logFileName = argv[0]+log;
    logFileName.erase(0, 2);

    // Start measuring time
    auto begin = std::chrono::high_resolution_clock::now();

	//check flags
    for (int i = 2; i < argc; i+=2){
        if(!strcmp(argv[i], "-d") || !strcmp(argv[i], "--dir")){
            directory_path = argv[i+1];
        }
        else if(!strcmp(argv[i], "-l") || !strcmp(argv[i], "--log_file")){
            logFileName=argv[i+1]+log;
        }
        else if(!strcmp(argv[i], "-r") || !strcmp(argv[i], "--result_file")){
            resultFileName=argv[i+1]+txt;
        }
        else if(!strcmp(argv[i], "-t") || !strcmp(argv[i], "--threads")){
        	if (stoi(argv[i+1])<1 || stoi(argv[i+1])>thread::hardware_concurrency()) {
        	num_threads=4;
        	}
        	else{
        	num_threads=stoi(argv[i+1]);
        	}
        }
        else{
            std::cerr << "Wrong flag.\n";
            return 1;
        }
    }

    //create resultFile
    resultFile.open(resultFileName, std::ios_base::app);

    //add time and date at the start of the file
    auto end2 = std::chrono::system_clock::now();
    std::time_t end_time = std::chrono::system_clock::to_time_t(end2);
    resultFile<<endl<<ctime(&end_time)<<endl;

    //create logFile
    logFile.open(logFileName, std::ios_base::app);
    logFile<<endl<<std::ctime(&end_time)<<endl;

    //search_string = argv[1];
    if (!fs::exists(directory_path)) {
        cout << "Directory " << directory_path << " does not exist." << endl;
        return 1;
    }

    file_queue.push(directory_path);

    vector<thread> thread_pool;
    for (int i = 0; i < num_threads; i++) {
        thread_pool.emplace_back(search_files);
    }

    for (auto& thread : thread_pool) {
        thread.join();
    }

    // Stop measuring time and calculate the elapsed time
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);

    std::cout << "Searched files: " << files << '\n';
    std::cout << "Files with pattern: " << nfiles << '\n';
    std::cout << "Patterns number: " << patterns << '\n';
    std::cout << "Result file: " << resultFileName << '\n';
    std::cout << "Log file: " << logFileName << '\n';
    std::cout << "Used threads: " << num_threads << '\n';
    printf("Time measured: %.6f seconds.\n", elapsed.count() * 1e-9);

    resultFile.close();

    return 0;
}