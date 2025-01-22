
#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <vector>
#include <fstream>
#include <regex>
#include <random>
#include <algorithm>
#include <iterator>
#include "json.hpp"
#include <iostream>

using json = nlohmann::json;

std::string wstringToString(const WCHAR* wideString) {
    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wideString, -1, nullptr, 0, nullptr, nullptr);
    std::string result(sizeNeeded, 0);
    WideCharToMultiByte(CP_UTF8, 0, wideString, -1, &result[0], sizeNeeded, nullptr, nullptr);
    result.pop_back(); // Supprimer le caractère nul final
    return result;
}

struct ProcessInfo {
    std::string name;
    std::vector<std::pair<DWORD, bool>> pids; // PID et statut "checked"
    std::string categorie;
};

// Fonction pour récupérer tous les processus et leurs PID
std::vector<ProcessInfo> getProcesses() {
    std::vector<ProcessInfo> processes;

    // Prendre un instantané des processus
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return processes;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &pe32)) {
        do {
            // Vérifier si le processus existe déjà dans la liste
            auto it = std::find_if(processes.begin(), processes.end(), [&pe32](const ProcessInfo& proc) {
                return proc.name == wstringToString(pe32.szExeFile);
                });

            if (it != processes.end()) {
                // Ajouter le PID à un processus existant
                it->pids.emplace_back(pe32.th32ProcessID, true);
            }
            else {
                // Ajouter un nouveau processus
                ProcessInfo info;
                info.name = wstringToString(pe32.szExeFile);
                info.pids.emplace_back(pe32.th32ProcessID, true);
                info.categorie = "Other";
                processes.push_back(info);
            }
        } while (Process32Next(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
    return processes;
}

std::string determineCategory(const std::string& processName) {
    // Liste des noms de navigateurs
    std::vector<std::string> browsers = {
        "chrome",
        "firefox",
        "msedge",
        "opera"
    };

    std::vector<std::string> offices = {
    "MSWORD",
    "soffice",
    "POWERPNT",
    "EXCEL"
    };

    // Vérifier si le nom du processus correspond à l'un des navigateurs
    for (const auto& browser : browsers) {
        std::regex pattern(browser, std::regex_constants::icase);
        if (std::regex_search(processName, pattern)) {
            return "Browser";
        }
    }
    // Vérifier si le nom du processus correspond à l'un des navigateurs
    for (const auto& office : offices) {
        std::regex pattern(office, std::regex_constants::icase);
        if (std::regex_search(processName, pattern)) {
            return "OfficeApplication";
        }
    }

    return ""; // Retourne une catégorie vide par défaut
}

std::vector<std::vector<int>> getDiffPid(json list1, json list2) {
    std::vector<int> listOld;
    std::vector<int> listNew;
    std::vector<int> diffTemp1;
    std::vector<int> diffTemp2;
    std::vector<std::vector<int>> output(2);
    
    for (int i = 0; i < list1.size();i++ ) {
        listOld.push_back(list1[i]["numeroPid"]);
    }

    for (int i = 0; i < list1.size(); i++) {
        listNew.push_back(list2[i]["numeroPid"]);
    }

    std::sort(listOld.begin(), listOld.end());
    std::sort(listNew.begin(), listNew.end());
    
    std::set_difference(listOld.begin(), listOld.end(), listNew.begin(), listNew.end(), std::inserter(diffTemp1, diffTemp1.begin()));
    std::set_difference(listNew.begin(), listNew.end(), listOld.begin(), listOld.end(), std::inserter(diffTemp2, diffTemp2.begin()));

    for (int i = 0; i < diffTemp1.size(); i++) { //remove
        output[1].push_back(diffTemp1[i]);
    }
    for (int i = 0; i < diffTemp2.size(); i++) { //add
        output[0].push_back(diffTemp2[i]);
    }

    return output;
}

std::vector<std::vector<std::string>> getDiffProcesses(std::vector<std::string>& list1, std::vector<std::string>& list2) {
    std::vector<std::string> diffTemp1;
    std::vector<std::string> diffTemp2;
    std::sort(list1.begin(), list1.end());
    std::sort(list2.begin(), list2.end());
    std::vector<std::vector<std::string>> output(2);
    std::set_difference(list1.begin(), list1.end(), list2.begin(), list2.end(),std::inserter(diffTemp1, diffTemp1.begin()));
    std::set_difference(list2.begin(), list2.end(), list1.begin(), list1.end(),std::inserter(diffTemp2, diffTemp2.begin()));

    std::cout << "Programme différent : " << std::endl << std::endl;

    for (int i = 0; i < diffTemp1.size(); i++) {
        std::cout << "Retirer : " << diffTemp1[i] << std::endl;
        output[1].push_back(diffTemp1[i]);
    }

    for (int i = 0; i < diffTemp2.size(); i++) {
        std::cout << "Ajout : " << diffTemp2[i] << std::endl;
        output[0].push_back(diffTemp2[i]);
    }

    std::cout << std::endl;

    return output;
}

json addElemProcessToJson(json input, const ProcessInfo process) {
    json pidArray = json::array();
    for (const auto& pid : process.pids) {
        pidArray.push_back({ {"numeroPid", pid.first}, {"checked", false} });
    }

    // Création d'un objet JSON avec l'ordre souhaité
    json processJson = json::object({
        {"name", process.name},
        {"pids", pidArray},
        {"categorie", determineCategory(process.name)},
        });

    input.push_back(processJson);
    std::cout << input.dump(4) << std::endl;
    return input;
}


json removeElemFromJsonFromKeyValue(json input, const std::string key, const std::string value) {
    for (auto it = input.begin(); it != input.end(); ) {
        if (it->contains(key) && (*it)[key] == value) {
            it = input.erase(it); // Remove the entry and update iterator
        }
        else {
            ++it; // Move to the next entry
        }
    }
    return input;
}

json writeInitialProcessesToJson(const std::vector<ProcessInfo>& processes) {
    json output;


    for (const auto& process : processes) {
        json pidArray = json::array();
        for (const auto& pid : process.pids) {
            pidArray.push_back({ {"numeroPid", pid.first}, {"checked", false} });
        }

        // Création d'un objet JSON avec l'ordre souhaité
        json processJson = json::object({
            {"name", process.name},
            {"pids", pidArray},
            {"categorie", determineCategory(process.name)}
            });
        output.push_back(processJson);
    }

    return output;
}

json updatePid(json input, const ProcessInfo process) {
    json oldPids;
    json addedPid;
    for (int i = 0; i < input.size(); i++ ) {
        if (input[i]["name"] == process.name) {
            oldPids = input[i]["pids"];
        }
    }
    json pidArray = json::array();
    for (const auto& pid : process.pids) {
        pidArray.push_back({ {"numeroPid", pid.first}, {"checked", pid.second} });
    }
    std::vector<std::vector<int>> diff = getDiffPid(oldPids, pidArray);

    // Remove PID
    for (auto it = input.begin(); it != input.end(); ) {
        if (it->contains("name") && (*it)["name"] == process.name) {
            for (int i = 0; i < diff[1].size(); i++) {
                for (auto it2 = it->at("pids").begin(); it2 != it->at("pids").end(); ) {
                    if (it->contains("numeroPid") && (*it)["numeroPid"] == diff[1][i]) {
                        it = input.erase(it); // Remove the entry and update iterator
                    }
                    else {
                        ++it2; // Move to the next entry
                    }
                }
            }
            for (int i = 0; i < diff[0].size(); i++) {
                addedPid = json::array();
                for (const auto& pid : process.pids) {
                    addedPid.push_back({ {"numeroPid", diff[0][i]}, {"checked", false}});
                }
            }
            it->at("pids").push_back(addedPid);
        }
        else {
            ++it;

        }
    }
    return input;
}

std::vector<std::pair<std::string, std::vector<int>>> getCheckedPid(const json input) {
    std::vector<std::pair<std::string, std::vector<int>>> output;
    for (int i = 0; i < input.size();i++) {
        std::pair<std::string, std::vector<int>> pairTemp;
        std::vector<int> listTemp;
        bool trouve = false;
        for (int j = 0; j < input[i]["pids"].size(); j++) {
            if (input[i]["pids"][j]["checked"] == true) {
                trouve = true;
                listTemp.push_back(input[i]["pids"][j]["numeroPid"]);
            }
        }
        if (trouve) {
            pairTemp.first = input[i]["name"];
            pairTemp.second = listTemp;
            output.push_back(pairTemp);
        }
    }
    return output;
}

json updateJson(json input, std::vector<std::pair<std::string, std::vector<int>>> listChecked) {
    for (int i = 0; i < input.size(); i++) {
        //Parcours chaque groupe de processus

        for (int j = 0; j < listChecked.size(); j++) {
            //Parcours la liste des processus précédemment checker
            
            if (input[i]["name"] == listChecked[j].first) {
                //Quand le nom des processus correspond à des processus checker
                for (int k = 0; k < input[i]["pids"].size(); k++) {
                    if (std::find(listChecked[j].second.begin(), listChecked[j].second.end(), input[i]["pids"][k]["numeroPid"]) != listChecked[j].second.end()) {
                        std::cout << "Nom groupe (newJSON) : " << input[i]["name"] << std::endl;
                        std::cout << "Nom groupe (listCheck) : " << listChecked[j].first << std::endl;
                        std::cout << "NumeroPid (newJSON) : " << input[i]["pids"][k]["numeroPid"] << std::endl;
                        std::cout << "NumPid (listCheck) : " << listChecked[j].second[0] << std::endl;
                        input[i]["pids"][k]["checked"] = true;
                    }
                }
            }
        } 
    }
    std::cout << "Coucou je suis là";
    return input;
}


    // Fonction pour écrire les processus dans un fichier JSON
    void writeProcessesToJsonExistingJson(const std::vector<ProcessInfo>&processes, const std::string & filename) {
        std::ifstream f(filename);
        json input = json::parse(f);
        json output;
        std::vector<std::string> listProcessNameNew;
        std::vector<std::string> listProcessNameOld;

        for (int i = 0; i < input.size(); i++)
        {
            listProcessNameOld.push_back(input[i]["name"]);
        }
        for (const auto& process : processes) {
            listProcessNameNew.push_back(process.name);
        }

        std::vector<std::vector<std::string>> diff = getDiffProcesses(listProcessNameOld, listProcessNameNew); // List of difference between the two jsons

        for (int i = 0; i < diff[1].size(); i++) {
            std::cout << diff[1][i] << std::endl;
            input = removeElemFromJsonFromKeyValue(input, "name", diff[1][i]);
        }


        for (const auto& process : processes) {
            if (std::find(diff[0].begin(), diff[0].end(), process.name) != diff[0].end()) {
                input = addElemProcessToJson(input, process);
            }
            input = updatePid(input, process);
        }
        // Écrire dans le fichier
        /*
        std::ofstream file(filename);
        if (file.is_open()) {
            file << input.dump(4); // Beautifier avec une indentation de 4
            file.close();
        }*/
}

int main(int argc, char* argv[]) 
{
    // Récupérer les processus
    std::vector<ProcessInfo> processes = getProcesses();
    json output;

    // Chemin par défaut du fichier JSON
    std::string filename = "processes.json";

    // Si un argument est fourni, utiliser ce chemin
    if (argc > 1) 
    {
        filename = argv[1];
    }

    std::ifstream f(filename);
    if (f.good()) {
        json oldJson = json::parse(f);
        json newJson = writeInitialProcessesToJson(processes);
        std::vector<std::pair<std::string, std::vector<int>>> listCheckedPid = getCheckedPid(oldJson);
        newJson = updateJson(newJson, listCheckedPid);
        std::ofstream file(filename);
        if (file.is_open()) {
            file << newJson.dump(4); // Beautifier avec une indentation de 4
            file.close();
        }
    }
    else {
        json initJson = writeInitialProcessesToJson(processes);
        std::ofstream file(filename);
        if (file.is_open()) {
            file << initJson.dump(4); // Beautifier avec une indentation de 4
            file.close();
        }
    }
    std::cout << "Json File Generated: " << filename << std::endl;
    return 0;
}