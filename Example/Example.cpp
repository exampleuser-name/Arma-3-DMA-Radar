#include <iostream>
#include <DMALibrary/Memory/Memory.h>
#include <thread>
#include <chrono>
#include <iomanip>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>
#include <cstdint>
#include <atomic>
#include <mutex>
#include <sstream>
#include <algorithm>
#include <utility>

constexpr int MAX_THREADS = 6;
constexpr int ENTITY_TYPES = 4;
constexpr int READ_DELAY_MS = 1;
constexpr float MIN_COORD_VALUE = 10.0f;
const std::string OUTPUT_PATH = "C:\\Arm\\";
//const std::string OUTPUT_PATH = "\\\\192.168.1337.1337\\Arma3\\";

struct Point {
    float x;
    float y;
    float view_x;
    float view_y;
    float view_z;
    int32_t network_id;
    std::string transport;
    int32_t type;
};

struct Inpoint {
    int64_t address;
};

struct PredSoilder {
    int64_t address;
};

struct Soilder {
    int64_t address;
};

struct TransportSoilder {
    int64_t address;
};

const DWORD local_pl[] = { 0x21C8850, 0x2C68, 0x08, 0xD0 };
const DWORD Network[] = { 0x2181628, 0x48, 0x38, 0x40 };

std::mutex file_mutex;

std::string numberToByteString(int64_t number) {
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&number);
    std::string result;
    result.reserve(sizeof(int64_t));

    for (size_t i = 0; i < sizeof(int64_t); ++i) {
        if (bytes[i] >= 32 && bytes[i] <= 126) {
            result.push_back(static_cast<char>(bytes[i]));
        }
    }
    return result;
}

inline bool IsScientificNotation(float value) {
    return std::fabs(value) > 1e10f;
}

std::string escapeJson(const std::string& str) {
    std::string escaped;
    escaped.reserve(str.size() * 1.1); 

    for (char c : str) {
        switch (c) {
        case '\"': escaped += "\\\""; break;
        case '\\': escaped += "\\\\"; break;
        case '/':  escaped += "\\/";  break;
        case '\b': escaped += "\\b";  break;
        case '\f': escaped += "\\f";  break;
        case '\n': escaped += "\\n";  break;
        case '\r': escaped += "\\r";  break;
        case '\t': escaped += "\\t";  break;
        default:   escaped += c;      break;
        }
    }
    return escaped;
}

void WriteCoordsToJson(const std::string& path, const std::vector<Point>& points) {
    std::lock_guard<std::mutex> lock(file_mutex);
    std::ofstream outFile(path);
    if (!outFile.is_open()) {
        std::cerr << "Failed to open file for writing: " << path << std::endl;
        return;
    }


    std::stringstream ss;
    ss << "{\n\"points\": [\n";

    for (size_t i = 0; i < points.size(); ++i) {
        const auto& p = points[i];
        ss << "    {\"x\": " << p.x
            << ", \"y\": " << p.y
            << ", \"view_x\": " << p.view_x
            << ", \"view_y\": " << p.view_y
            << ", \"view_z\": " << p.view_z
            << ", \"network_id\": \"" << p.network_id
            << "\", \"transport\": \"" << escapeJson(p.transport)
            << "\", \"type\": " << p.type << "}";

        if (i < points.size() - 1) ss << ",";
        ss << "\n";
    }

    ss << "]}\n";
    outFile << ss.str();
    outFile.close();
}



void WriteInpoint(const std::string& path, const std::vector<Inpoint>& inpoint) {
    std::lock_guard<std::mutex> lock(file_mutex);
    std::ofstream outFile(path);
    if (!outFile.is_open()) {
        std::cerr << "Failed to open file for writing: " << path << std::endl;
        return;
    }


    std::stringstream ss;
    ss << "{\n\"points\": [\n";

    for (size_t i = 0; i < inpoint.size(); ++i) {
        const auto& inp = inpoint[i];
        ss << "    {\"x\": " << inp.address << "}";

        if (i < inpoint.size() - 1) ss << ",";
        ss << "\n";
    }

    ss << "]}\n";
    outFile << ss.str();
    outFile.close();
}

void WriteNetworkClientsToJson(const std::string& path, const std::vector<std::pair<int32_t, std::string>>& players) {
    std::lock_guard<std::mutex> lock(file_mutex);
    std::ofstream outFile(path);
    if (!outFile.is_open()) {
        std::cerr << "Failed to open file for writing: " << path << std::endl;
        return;
    }

    std::stringstream ss;
    ss << "{\n\"network_players\": [\n";

    for (size_t i = 0; i < players.size(); ++i) {
        ss << "    {\"identity\": " << players[i].first
            << ", \"name\": \"" << escapeJson(players[i].second) << "\"}";

        if (i < players.size() - 1) ss << ",";
        ss << "\n";
    }

    ss << "]}\n";
    outFile << ss.str();
    outFile.close();
}



void WriteLocalPlayerToJson(const std::string& path, const Point& point) {
    std::lock_guard<std::mutex> lock(file_mutex);
    std::ofstream outFile(path);
    if (!outFile.is_open()) {
        std::cerr << "Failed to open file for writing: " << path << std::endl;
        return;
    }

    std::stringstream ss;
    ss << "{\n\"local_player\": {\n"
        << "    \"x\": " << point.x << ",\n"
        << "    \"y\": " << point.y << ",\n"
        << "    \"view_x\": " << point.view_x << ",\n"
        << "    \"view_y\": " << point.view_y << ",\n"
        << "    \"view_z\": " << point.view_z << ",\n"
        << "    \"network_id\": \"" << point.network_id << "\",\n"
        << "    \"type\": " << point.type << ",\n"
        << "    \"transport\": \"" << escapeJson(point.transport) << "\"\n"
        << "}}\n";

    outFile << ss.str();
    outFile.close();
}

void GetLocalPlayer(Memory& mem, uintptr_t base, const DWORD local_pl[]) {

    int64_t local_player_road = mem.Read<int64_t>(base + local_pl[0]);
    int64_t local_player_road1 = mem.Read<int64_t>(local_player_road + local_pl[1]);
    int64_t local_player_road2 = mem.Read<int64_t>(local_player_road1 + local_pl[2]);
    int64_t local_player_road3 = mem.Read<int64_t>(local_player_road2 + local_pl[3]);
    int64_t local_player_road_view_x = mem.Read<int64_t>(local_player_road3 + 0x8);
    int64_t local_player_road_view_y = mem.Read<int64_t>(local_player_road3 + 0x10);
    int64_t local_player_road_view_z = mem.Read<int64_t>(local_player_road3 + 0x20);
    int64_t local_player_x = mem.Read<int64_t>(local_player_road3 + 0x2C);
    int64_t local_player_y = mem.Read<int64_t>(local_player_road3 + 0x34);
    float local_finalX = *reinterpret_cast<float*>(&local_player_x);
    float local_finalY = *reinterpret_cast<float*>(&local_player_y);
    float sub_finalview_x = *reinterpret_cast<float*>(&local_player_road_view_x);
    float sub_finalview_y = *reinterpret_cast<float*>(&local_player_road_view_y);
    float sub_finalview_z = *reinterpret_cast<float*>(&local_player_road_view_z);
    if (local_finalX >= MIN_COORD_VALUE && local_finalY >= MIN_COORD_VALUE &&
        !IsScientificNotation(local_finalX) && !IsScientificNotation(local_finalY)) {
        Point localPlayer = Point{ local_finalX, local_finalY, sub_finalview_x, sub_finalview_y, sub_finalview_z, 1337, "BEBRAEDINA", 1337 };
        WriteLocalPlayerToJson(OUTPUT_PATH + "local_player.json", localPlayer);
    }
}

void GetNetworkClients(Memory& mem, uintptr_t base) {
    auto handle = mem.CreateScatterHandle();
    std::vector<std::pair<int32_t, std::string>> players;
    int64_t NetworkManager = mem.Read<int64_t>(base + Network[0]);
    int64_t NetworkClient = mem.Read<int64_t>(NetworkManager + Network[1]);
    int64_t NetworkClientIdentities = mem.Read<int64_t>(NetworkClient + Network[2]);
    int32_t NetworkClient_Size = mem.Read<int32_t>(NetworkClient + Network[3]);
    players.reserve(NetworkClient_Size);
    for (int clientID = 0; clientID < NetworkClient_Size; ++clientID) {
        int32_t NetworkClient_Indentityes_Player = mem.Read<int32_t>(NetworkClientIdentities + 0x8 + 0x290 * clientID);
        int64_t NetworkClient_Player_Name_Road = mem.Read<int64_t>(NetworkClientIdentities + 0x150 + clientID * 0x290);
        int32_t NetworkClient_Player_Name_Array = mem.Read<int32_t>(NetworkClient_Player_Name_Road + 0x08);
        char NetworkClient_Player_Name[50];
        mem.AddScatterReadRequest(handle, NetworkClient_Player_Name_Road + 0x10, &NetworkClient_Player_Name, sizeof(NetworkClient_Player_Name));
        mem.ExecuteReadScatter(handle);
        players.emplace_back(NetworkClient_Indentityes_Player, std::move(NetworkClient_Player_Name));
    }
    WriteNetworkClientsToJson(OUTPUT_PATH + "netplayer.json", players);
    mem.CloseScatterHandle(handle);
}

std::vector<Inpoint> inpoint;
std::vector<Soilder> soilder;
std::vector<TransportSoilder> transportsoilders;
std::vector<PredSoilder> predsoilder;
void Optimization(Memory& mem, uintptr_t base) {
    std::vector<Point> points;
    points.reserve(1000);
    int sizeAddress = inpoint.size();
    float sub_player_inX, sub_player_inY;
    float sub_player_view_x, sub_player_view_y, sub_player_view_z;
    int32_t sub_player_Network_id;
    int32_t teamID;
    byte isDead;
    byte intransport;
    int32_t counter_subgroup_01_subs;
    int64_t counter_subgroup_01;

    for (int i = 0; i < sizeAddress; i++) {
        auto handle = mem.CreateScatterHandle();
        int64_t countVector = soilder[i].address;
        int64_t predVector = predsoilder[i].address;
        int64_t sub_player_in4 = mem.Read<int64_t>(countVector + 0xD0);
        mem.AddScatterReadRequest(handle, sub_player_in4 + 0x2C, &sub_player_inX, sizeof(float));
        mem.AddScatterReadRequest(handle, sub_player_in4 + 0x34, &sub_player_inY, sizeof(float));
        mem.AddScatterReadRequest(handle, sub_player_in4 + 0x08, &sub_player_view_x, sizeof(float));
        mem.AddScatterReadRequest(handle, sub_player_in4 + 0x10, &sub_player_view_y, sizeof(float));
        mem.AddScatterReadRequest(handle, sub_player_in4 + 0x20, &sub_player_view_z, sizeof(float));
        mem.AddScatterReadRequest(handle, countVector + 0xC34, &sub_player_Network_id, sizeof(int32_t));
        mem.AddScatterReadRequest(handle, countVector + 0x434, &teamID, sizeof(int32_t));
        mem.AddScatterReadRequest(handle, countVector + 0x5CC, &isDead, sizeof(byte)); //old 0x4C4  BB8 5CC
        mem.AddScatterReadRequest(handle, sub_player_in4 + 0xC, &intransport, sizeof(byte)); //old 0x4C4  BB8 5CC
        mem.ExecuteReadScatter(handle);
        
        std::string Player_Transport_Name;
        if (intransport != 0) {
            char Player_Transport_Name[50];
            mem.AddScatterReadRequest(handle, transportsoilders[i].address + 0x10, &Player_Transport_Name, sizeof(Player_Transport_Name));
            mem.ExecuteReadScatter(handle);
            points.emplace_back(Point{ sub_player_inX, sub_player_inY, sub_player_view_x, sub_player_view_y, sub_player_view_z,
                                   sub_player_Network_id, std::move(Player_Transport_Name), isDead });
        }

        if (isDead % 2) {
        }
        else {
            isDead = teamID;
        }
        points.emplace_back(Point{ sub_player_inX, sub_player_inY, sub_player_view_x, sub_player_view_y, sub_player_view_z,
                                     sub_player_Network_id, std::move(Player_Transport_Name), isDead });
        mem.CloseScatterHandle(handle);

    }
    std::string filename = OUTPUT_PATH + "coords" + std::to_string(0) + ".json";
    WriteCoordsToJson(filename, points);
}

int countoptimaize = 0;
void ProcessEntities_road(Memory& mem, uintptr_t base, int j, int32_t counter_offsets, int64_t base_entity_ai_part_3) {
    auto handle = mem.CreateScatterHandle();
    float sub_player_inX, sub_player_inY;
    float sub_player_view_x, sub_player_view_y, sub_player_view_z;
    int32_t sub_player_Network_id;
    byte isDead;
    int32_t counter_subgroup_01_subs;
    int64_t counter_subgroup_01;
    std::vector<Point> points;
    points.reserve(1000);
    for (int i = 0; i < counter_offsets; ++i) {
        int64_t player_info_road_1 = mem.Read<int64_t>(base_entity_ai_part_3 + 0x0 + 0x08 * i);
        mem.AddScatterReadRequest(handle, player_info_road_1 + 0x178, &counter_subgroup_01_subs, sizeof(int32_t));
        mem.AddScatterReadRequest(handle, player_info_road_1 + 0x170, &counter_subgroup_01, sizeof(int64_t));
        mem.ExecuteReadScatter(handle);
        for (int g = 0; g < counter_subgroup_01_subs; ++g) {
            int64_t counter_subgroup_02 = mem.Read<int64_t>(counter_subgroup_01 + 0x38 * g);
            int64_t new_counter_subgroup_01 = mem.Read<int64_t>(counter_subgroup_02 + 0x8);
            int64_t sub_player_in2 = mem.Read<int64_t>(new_counter_subgroup_01 + 0xC8);
            int64_t sub_player_in3 = mem.Read<int64_t>(sub_player_in2 + 0x8);
            int64_t sub_player_Transport_road_0 = mem.Read<int64_t>(new_counter_subgroup_01 + 0xD0);
            int64_t sub_player_in_Transport = mem.Read<int64_t>(sub_player_Transport_road_0 + 0x8);
            int64_t sub_player_Transport_road_2 = mem.Read<int64_t>(sub_player_in_Transport + 0x150);
            int64_t sub_player_Transport_road_3 = mem.Read<int64_t>(sub_player_Transport_road_2 + 0x13B8);// or 13С0
            transportsoilders.emplace_back(TransportSoilder{ sub_player_Transport_road_3 });
            inpoint.emplace_back(Inpoint{ new_counter_subgroup_01 });
            soilder.emplace_back(Soilder{ sub_player_in3 });
            predsoilder.emplace_back(PredSoilder{ sub_player_in2 });
        }
    }
    mem.CloseScatterHandle(handle);

    countoptimaize++;
    if (countoptimaize == 4) {
        std::string inpointname = OUTPUT_PATH + "address" + ".json";
        WriteInpoint(inpointname, inpoint);
        countoptimaize = 0;
    }
}

std::pair<int32_t, int64_t> ProcessEntities(Memory& mem, uintptr_t base, int j, int64_t base_entity_ai_part_1) {
    int64_t base_entity_ai_part_2 = mem.Read<int64_t>(base_entity_ai_part_1 + 0x2B48 + 0x8 * j);
    int64_t base_entity_ai_part_3;
    int32_t counter_offsets;
    auto handle_coutersub = mem.CreateScatterHandle();
    mem.AddScatterReadRequest(handle_coutersub, base_entity_ai_part_2 + 0x48, &base_entity_ai_part_3, sizeof(int64_t));
    mem.AddScatterReadRequest(handle_coutersub, base_entity_ai_part_2 + 0x50, &counter_offsets, sizeof(int32_t));
    mem.ExecuteReadScatter(handle_coutersub);
    mem.CloseScatterHandle(handle_coutersub);
    ProcessEntities_road(mem, base, j, counter_offsets, base_entity_ai_part_3);
    return std::make_pair(counter_offsets, base_entity_ai_part_3);
}




int main() {
    setlocale(LC_ALL, "Russian");
    Memory mem;
    if (!mem.Init("Arma3_x64.exe", true, true)) {
        std::cerr << "Failed to initialize DMA" << std::endl;
        return 1;
    }

    std::cout << "DMA initialized" << std::endl;

    if (!mem.FixCr3()) {
        std::cerr << "Failed to fix CR3" << std::endl;
        return 1;
    }

    std::cout << "CR3 fixed" << std::endl;

    uintptr_t base = mem.GetBaseDaddy("Arma3_x64.exe");
    if (base == 0) {
        std::cerr << "Failed to get base address" << std::endl;
        return 1;
    }

    int64_t base_entity_ai_part_1 = mem.Read<int64_t>(base + 0x21C8850);

    std::atomic<bool> running = true;

    std::thread localPlayerThread([&]() {
        while (running) {
            GetLocalPlayer(mem, base, local_pl);
            std::this_thread::sleep_for(std::chrono::milliseconds(READ_DELAY_MS));
        }
        });

    std::thread networkClientsThread([&]() {
        while (running) {
            GetNetworkClients(mem, base);
            std::this_thread::sleep_for(std::chrono::seconds(100));

        }
        });


    std::thread ProcessEntitiesThread([&]() {
        while (running) {
            inpoint.clear();
            soilder.clear();
            transportsoilders.clear();
            ProcessEntities(mem, base, 0, base_entity_ai_part_1);
            ProcessEntities(mem, base, 1, base_entity_ai_part_1);
            ProcessEntities(mem, base, 2, base_entity_ai_part_1);
            ProcessEntities(mem, base, 3, base_entity_ai_part_1);
            std::this_thread::sleep_for(std::chrono::seconds(30));
        }
        });


    std::thread OptimazeMethod([&]() {
        while (running) {
            Optimization(mem, base);
            std::this_thread::sleep_for(std::chrono::milliseconds(READ_DELAY_MS));
        }
        });

    std::cout << "Press Enter to stop..." << std::endl;
    std::cin.get();


    running = false;

    //localPlayerThread.detach();
    //networkClientsThread.detach();
    //GetTransortPlayerThread.detach();
    //ProcessEntitiesThread.detach();

    return 0;
}