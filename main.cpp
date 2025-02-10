#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>

// File names (binary files with fixed-length records)
const char* MASTER_FILE = "S.fl";             // Master file (suppliers)
const char* SLAVE_FILE  = "SP.fl";              // Slave file (supplies)
const char* INDEX_FILE  = "S.ind";              // Index table for S.fl
const char* MASTER_GARBAGE_FILE = "S.garbage";  // Garbage zone for master file
const char* SLAVE_GARBAGE_FILE  = "SP.garbage";  // Garbage zone for slave file

// Structure for a supplier (master record)
// Fields:
//  supplierID (key), surname, status, city,
//  firstSupply (record number in SP.fl for first supply, -1 if none),
//  supplyCount (number of supplies),
//  valid (1 - record exists, 0 - logically deleted)
struct Supplier {
    int supplierID;
    char surname[31];
    int status;
    char city[31];
    int firstSupply;   // Number of the first supply record in SP.fl (-1 if none)
    int supplyCount;
    int valid;         // 1 = exists, 0 = deleted
};

// Structure for a supply record (slave record)
// Fields:
//  supplierID (foreign key), detailID, price, quantity,
//  nextSupply (number of the next supply record in the chain, -1 if none),
//  valid (1 - exists, 0 - logically deleted)
struct Supply {
    int supplierID;
    int detailID;
    double price;
    int quantity;
    int nextSupply;    // Next record in SP.fl (-1 if none)
    int valid;         // 1 = exists, 0 = deleted
};

// Structure for an index table record (for S.fl)
struct IndexRecord {
    int supplierID;
    int recordNumber;  // Record number in S.fl
};

std::vector<IndexRecord> indexTable;
std::vector<int> masterGarbage; // Record numbers of logically deleted Supplier records in S.fl
std::vector<int> slaveGarbage;  // Record numbers of logically deleted Supply records in SP.fl

// ===================== INDEX AND GARBAGE HANDLING =====================
void loadIndexTable() {
    indexTable.clear();
    std::ifstream in(INDEX_FILE, std::ios::binary);
    if (!in) {
        // If the index table file does not exist, scan S.fl to build it.
        std::ifstream master(MASTER_FILE, std::ios::binary);
        if (!master) return;
        Supplier supp;
        int recNum = 0;
        while (master.read(reinterpret_cast<char*>(&supp), sizeof(Supplier))) {
            if (supp.valid == 1) {
                IndexRecord ir;
                ir.supplierID = supp.supplierID;
                ir.recordNumber = recNum;
                indexTable.push_back(ir);
            }
            recNum++;
        }
        master.close();
        std::sort(indexTable.begin(), indexTable.end(), [](const IndexRecord &a, const IndexRecord &b) {
            return a.supplierID < b.supplierID;
        });
        return;
    }
    IndexRecord temp;
    while (in.read(reinterpret_cast<char*>(&temp), sizeof(IndexRecord))) {
        indexTable.push_back(temp);
    }
    in.close();
}

void saveIndexTable() {
    std::ofstream out(INDEX_FILE, std::ios::binary | std::ios::trunc);
    for (auto &ir : indexTable)
        out.write(reinterpret_cast<char*>(&ir), sizeof(IndexRecord));
    out.close();
}

void loadMasterGarbage() {
    masterGarbage.clear();
    std::ifstream in(MASTER_GARBAGE_FILE, std::ios::binary);
    if (!in) return;
    int rec;
    while (in.read(reinterpret_cast<char*>(&rec), sizeof(int)))
        masterGarbage.push_back(rec);
    in.close();
}

void loadSlaveGarbage() {
    slaveGarbage.clear();
    std::ifstream in(SLAVE_GARBAGE_FILE, std::ios::binary);
    if (!in) return;
    int rec;
    while (in.read(reinterpret_cast<char*>(&rec), sizeof(int)))
        slaveGarbage.push_back(rec);
    in.close();
}

void saveMasterGarbage() {
    std::ofstream out(MASTER_GARBAGE_FILE, std::ios::binary | std::ios::trunc);
    for (auto &rec : masterGarbage)
        out.write(reinterpret_cast<char*>(&rec), sizeof(int));
    out.close();
}

void saveSlaveGarbage() {
    std::ofstream out(SLAVE_GARBAGE_FILE, std::ios::binary | std::ios::trunc);
    for (auto &rec : slaveGarbage)
        out.write(reinterpret_cast<char*>(&rec), sizeof(int));
    out.close();
}

// ===================== GET FUNCTIONS =====================

// get-m: Read master record by supplierID and display its fields.
void getMaster() {
    int supplierID;
    std::cout << "Enter Supplier ID: ";
    std::cin >> supplierID;
    
    // Binary search in the index table (index table is sorted by supplierID)
    auto it = std::lower_bound(indexTable.begin(), indexTable.end(), supplierID,
        [](const IndexRecord &ir, int id) { return ir.supplierID < id; });
    if (it == indexTable.end() || it->supplierID != supplierID) {
        std::cout << "Supplier not found." << std::endl;
        return;
    }
    int recNum = it->recordNumber;
    std::fstream file(MASTER_FILE, std::ios::binary | std::ios::in);
    if (!file) {
        std::cerr << "Error opening master file." << std::endl;
        return;
    }
    file.seekg(recNum * sizeof(Supplier));
    Supplier supp;
    file.read(reinterpret_cast<char*>(&supp), sizeof(Supplier));
    file.close();
    
    if (supp.valid == 0) {
        std::cout << "Supplier record is deleted." << std::endl;
        return;
    }
    std::cout << "\nSupplier Record:" << std::endl;
    std::cout << "Supplier ID: " << supp.supplierID << std::endl;
    std::cout << "Surname: " << supp.surname << std::endl;
    std::cout << "Status: " << supp.status << std::endl;
    std::cout << "City: " << supp.city << std::endl;
    std::cout << "First Supply Index: " << supp.firstSupply << std::endl;
    std::cout << "Supply Count: " << supp.supplyCount << std::endl;
}

// get-s: Read slave record (supply) by supplierID and detailID by traversing the linked list.
void getSlave() {
    int supplierID, detailID;
    std::cout << "Enter Supplier ID: ";
    std::cin >> supplierID;
    std::cout << "Enter Detail ID: ";
    std::cin >> detailID;
    
    // Find the supplier via the index table
    auto it = std::lower_bound(indexTable.begin(), indexTable.end(), supplierID,
        [](const IndexRecord &ir, int id) { return ir.supplierID < id; });
    if (it == indexTable.end() || it->supplierID != supplierID) {
        std::cout << "Supplier not found." << std::endl;
        return;
    }
    int suppRecNum = it->recordNumber;
    std::fstream sfile(MASTER_FILE, std::ios::binary | std::ios::in);
    if (!sfile) {
        std::cerr << "Error opening master file." << std::endl;
        return;
    }
    sfile.seekg(suppRecNum * sizeof(Supplier));
    Supplier supp;
    sfile.read(reinterpret_cast<char*>(&supp), sizeof(Supplier));
    sfile.close();
    if (supp.valid == 0) {
        std::cout << "Supplier record is deleted." << std::endl;
        return;
    }
    
    int supplyIndex = supp.firstSupply;
    std::fstream spfile(SLAVE_FILE, std::ios::binary | std::ios::in);
    if (!spfile) {
        std::cerr << "Error opening slave file." << std::endl;
        return;
    }
    bool found = false;
    while (supplyIndex != -1) {
        spfile.seekg(supplyIndex * sizeof(Supply));
        Supply supRecord;
        spfile.read(reinterpret_cast<char*>(&supRecord), sizeof(Supply));
        if (supRecord.valid == 1 && supRecord.detailID == detailID) {
            std::cout << "\nSupply Record:" << std::endl;
            std::cout << "Supplier ID: " << supRecord.supplierID << std::endl;
            std::cout << "Detail ID: " << supRecord.detailID << std::endl;
            std::cout << "Price: " << supRecord.price << std::endl;
            std::cout << "Quantity: " << supRecord.quantity << std::endl;
            std::cout << "Next Supply Index: " << supRecord.nextSupply << std::endl;
            found = true;
            break;
        }
        supplyIndex = supRecord.nextSupply;
    }
    spfile.close();
    if (!found)
        std::cout << "Supply record not found." << std::endl;
}

// ===================== DELETE FUNCTIONS =====================

// del-m: Delete a master record (supplier) by supplierID and all its subordinate supply records.
void delMaster() {
    int supplierID;
    std::cout << "Enter Supplier ID to delete: ";
    std::cin >> supplierID;
    
    auto it = std::lower_bound(indexTable.begin(), indexTable.end(), supplierID,
        [](const IndexRecord &ir, int id) { return ir.supplierID < id; });
    if (it == indexTable.end() || it->supplierID != supplierID) {
        std::cout << "Supplier not found." << std::endl;
        return;
    }
    int suppRecNum = it->recordNumber;
    std::fstream mfile(MASTER_FILE, std::ios::binary | std::ios::in | std::ios::out);
    if (!mfile) {
        std::cerr << "Error opening master file." << std::endl;
        return;
    }
    mfile.seekg(suppRecNum * sizeof(Supplier));
    Supplier supp;
    mfile.read(reinterpret_cast<char*>(&supp), sizeof(Supplier));
    if (supp.valid == 0) {
        std::cout << "Supplier already deleted." << std::endl;
        mfile.close();
        return;
    }
    // Delete all subordinate supply records
    int supplyIndex = supp.firstSupply;
    std::fstream spfile(SLAVE_FILE, std::ios::binary | std::ios::in | std::ios::out);
    if (!spfile) {
        std::cerr << "Error opening slave file." << std::endl;
        mfile.close();
        return;
    }
    while (supplyIndex != -1) {
        spfile.seekg(supplyIndex * sizeof(Supply));
        Supply supplyRec;
        spfile.read(reinterpret_cast<char*>(&supplyRec), sizeof(Supply));
        if (supplyRec.valid == 1) {
            supplyRec.valid = 0;
            spfile.seekp(supplyIndex * sizeof(Supply));
            spfile.write(reinterpret_cast<char*>(&supplyRec), sizeof(Supply));
            slaveGarbage.push_back(supplyIndex);
        }
        supplyIndex = supplyRec.nextSupply;
    }
    spfile.close();
    // Mark supplier record as deleted
    supp.valid = 0;
    mfile.seekp(suppRecNum * sizeof(Supplier));
    mfile.write(reinterpret_cast<char*>(&supp), sizeof(Supplier));
    mfile.close();
    masterGarbage.push_back(suppRecNum);
    // Remove from index table
    indexTable.erase(it);
    std::cout << "Supplier and its supplies have been deleted." << std::endl;
}

// del-s: Delete a subordinate supply record (by supplierID and detailID).
void delSlave() {
    int supplierID, detailID;
    std::cout << "Enter Supplier ID for supply deletion: ";
    std::cin >> supplierID;
    std::cout << "Enter Detail ID of the supply to delete: ";
    std::cin >> detailID;
    
    auto it = std::lower_bound(indexTable.begin(), indexTable.end(), supplierID,
        [](const IndexRecord &ir, int id) { return ir.supplierID < id; });
    if (it == indexTable.end() || it->supplierID != supplierID) {
        std::cout << "Supplier not found." << std::endl;
        return;
    }
    int suppRecNum = it->recordNumber;
    std::fstream mfile(MASTER_FILE, std::ios::binary | std::ios::in | std::ios::out);
    if (!mfile) {
        std::cerr << "Error opening master file." << std::endl;
        return;
    }
    mfile.seekg(suppRecNum * sizeof(Supplier));
    Supplier supp;
    mfile.read(reinterpret_cast<char*>(&supp), sizeof(Supplier));
    if (supp.valid == 0) {
        std::cout << "Supplier record is deleted." << std::endl;
        mfile.close();
        return;
    }
    // Search for the supply in the linked list
    int currentIndex = supp.firstSupply;
    int prevIndex = -1;
    bool found = false;
    std::fstream spfile(SLAVE_FILE, std::ios::binary | std::ios::in | std::ios::out);
    if (!spfile) {
        std::cerr << "Error opening slave file." << std::endl;
        mfile.close();
        return;
    }
    while (currentIndex != -1) {
        spfile.seekg(currentIndex * sizeof(Supply));
        Supply supplyRec;
        spfile.read(reinterpret_cast<char*>(&supplyRec), sizeof(Supply));
        if (supplyRec.valid == 1 && supplyRec.detailID == detailID) {
            found = true;
            // If this is the first record in the chain
            if (prevIndex == -1) {
                supp.firstSupply = supplyRec.nextSupply;
            } else {
                // Update nextSupply of the previous record
                spfile.seekg(prevIndex * sizeof(Supply));
                Supply prevRec;
                spfile.read(reinterpret_cast<char*>(&prevRec), sizeof(Supply));
                prevRec.nextSupply = supplyRec.nextSupply;
                spfile.seekp(prevIndex * sizeof(Supply));
                spfile.write(reinterpret_cast<char*>(&prevRec), sizeof(Supply));
            }
            supplyRec.valid = 0;
            spfile.seekp(currentIndex * sizeof(Supply));
            spfile.write(reinterpret_cast<char*>(&supplyRec), sizeof(Supply));
            slaveGarbage.push_back(currentIndex);
            supp.supplyCount--;
            break;
        }
        prevIndex = currentIndex;
        currentIndex = supplyRec.nextSupply;
    }
    spfile.close();
    mfile.seekp(suppRecNum * sizeof(Supplier));
    mfile.write(reinterpret_cast<char*>(&supp), sizeof(Supplier));
    mfile.close();
    if (!found)
        std::cout << "Supply record not found." << std::endl;
    else
        std::cout << "Supply record deleted." << std::endl;
}

// ===================== UPDATE FUNCTIONS =====================

// update-m: Update a non-key field (surname, status, city) of a supplier record.
void updateMaster() {
    int supplierID;
    std::cout << "Enter Supplier ID to update: ";
    std::cin >> supplierID;
    
    auto it = std::lower_bound(indexTable.begin(), indexTable.end(), supplierID,
        [](const IndexRecord &ir, int id) { return ir.supplierID < id; });
    if (it == indexTable.end() || it->supplierID != supplierID) {
        std::cout << "Supplier not found." << std::endl;
        return;
    }
    int recNum = it->recordNumber;
    std::fstream mfile(MASTER_FILE, std::ios::binary | std::ios::in | std::ios::out);
    if (!mfile) {
        std::cerr << "Error opening master file." << std::endl;
        return;
    }
    mfile.seekg(recNum * sizeof(Supplier));
    Supplier supp;
    mfile.read(reinterpret_cast<char*>(&supp), sizeof(Supplier));
    if (supp.valid == 0) {
        std::cout << "Supplier record is deleted." << std::endl;
        mfile.close();
        return;
    }
    int choice;
    std::cout << "Select field to update:\n1. Surname\n2. Status\n3. City\nChoice: ";
    std::cin >> choice;
    switch(choice) {
        case 1:
            std::cout << "Enter new Surname: ";
            std::cin >> supp.surname;
            break;
        case 2:
            std::cout << "Enter new Status: ";
            std::cin >> supp.status;
            break;
        case 3:
            std::cout << "Enter new City: ";
            std::cin >> supp.city;
            break;
        default:
            std::cout << "Invalid choice." << std::endl;
            mfile.close();
            return;
    }
    mfile.seekp(recNum * sizeof(Supplier));
    mfile.write(reinterpret_cast<char*>(&supp), sizeof(Supplier));
    mfile.close();
    std::cout << "Supplier record updated." << std::endl;
}

// update-s: Update a non-key field (price or quantity) of a supply record.
void updateSlave() {
    int supplierID, detailID;
    std::cout << "Enter Supplier ID for supply update: ";
    std::cin >> supplierID;
    std::cout << "Enter Detail ID of supply to update: ";
    std::cin >> detailID;
    
    auto it = std::lower_bound(indexTable.begin(), indexTable.end(), supplierID,
        [](const IndexRecord &ir, int id) { return ir.supplierID < id; });
    if (it == indexTable.end() || it->supplierID != supplierID) {
        std::cout << "Supplier not found." << std::endl;
        return;
    }
    int suppRecNum = it->recordNumber;
    std::fstream mfile(MASTER_FILE, std::ios::binary | std::ios::in);
    if (!mfile) {
        std::cerr << "Error opening master file." << std::endl;
        return;
    }
    mfile.seekg(suppRecNum * sizeof(Supplier));
    Supplier supp;
    mfile.read(reinterpret_cast<char*>(&supp), sizeof(Supplier));
    mfile.close();
    if (supp.valid == 0) {
        std::cout << "Supplier record is deleted." << std::endl;
        return;
    }
    int currentIndex = supp.firstSupply;
    int targetIndex = -1;
    std::fstream spfile(SLAVE_FILE, std::ios::binary | std::ios::in | std::ios::out);
    if (!spfile) {
        std::cerr << "Error opening slave file." << std::endl;
        return;
    }
    Supply supplyRec;
    bool found = false;
    while (currentIndex != -1) {
        spfile.seekg(currentIndex * sizeof(Supply));
        spfile.read(reinterpret_cast<char*>(&supplyRec), sizeof(Supply));
        if (supplyRec.valid == 1 && supplyRec.detailID == detailID) {
            found = true;
            targetIndex = currentIndex;
            break;
        }
        currentIndex = supplyRec.nextSupply;
    }
    if (!found) {
        std::cout << "Supply record not found." << std::endl;
        spfile.close();
        return;
    }
    int choice;
    std::cout << "Select field to update:\n1. Price\n2. Quantity\nChoice: ";
    std::cin >> choice;
    switch(choice) {
        case 1:
            std::cout << "Enter new Price: ";
            std::cin >> supplyRec.price;
            break;
        case 2:
            std::cout << "Enter new Quantity: ";
            std::cin >> supplyRec.quantity;
            break;
        default:
            std::cout << "Invalid choice." << std::endl;
            spfile.close();
            return;
    }
    spfile.seekp(targetIndex * sizeof(Supply));
    spfile.write(reinterpret_cast<char*>(&supplyRec), sizeof(Supply));
    spfile.close();
    std::cout << "Supply record updated." << std::endl;
}

// ===================== INSERT FUNCTIONS =====================

// insert-m: Insert a new supplier record into S.fl, using the master garbage zone if available.
void insertMaster() {
    Supplier supp;
    std::cout << "Enter Supplier ID: ";
    std::cin >> supp.supplierID;
    std::cout << "Enter Surname: ";
    std::cin >> supp.surname;
    std::cout << "Enter Status: ";
    std::cin >> supp.status;
    std::cout << "Enter City: ";
    std::cin >> supp.city;
    supp.firstSupply = -1;
    supp.supplyCount = 0;
    supp.valid = 1;
    
    int recNum;
    // Use a free record from masterGarbage if available.
    if (!masterGarbage.empty()) {
        recNum = masterGarbage.back();
        masterGarbage.pop_back();
        std::fstream mfile(MASTER_FILE, std::ios::binary | std::ios::in | std::ios::out);
        if (!mfile) {
            std::cerr << "Error opening master file." << std::endl;
            return;
        }
        mfile.seekp(recNum * sizeof(Supplier));
        mfile.write(reinterpret_cast<char*>(&supp), sizeof(Supplier));
        mfile.close();
    } else {
        std::ofstream mfile(MASTER_FILE, std::ios::binary | std::ios::app);
        if (!mfile) {
            std::cerr << "Error opening master file." << std::endl;
            return;
        }
        mfile.write(reinterpret_cast<char*>(&supp), sizeof(Supplier));
        mfile.close();
        std::ifstream in(MASTER_FILE, std::ios::binary);
        in.seekg(0, std::ios::end);
        recNum = in.tellg() / sizeof(Supplier) - 1;
        in.close();
    }
    // Update the index table.
    IndexRecord ir;
    ir.supplierID = supp.supplierID;
    ir.recordNumber = recNum;
    indexTable.push_back(ir);
    std::sort(indexTable.begin(), indexTable.end(), [](const IndexRecord &a, const IndexRecord &b) {
        return a.supplierID < b.supplierID;
    });
    std::cout << "Supplier record inserted." << std::endl;
}

// insert-s: Insert a new supply record into SP.fl and link it as the first record in the supplier's chain.
void insertSlave() {
    int supplierID;
    std::cout << "Enter Supplier ID for the supply: ";
    std::cin >> supplierID;
    // Find the supplier.
    auto it = std::lower_bound(indexTable.begin(), indexTable.end(), supplierID,
        [](const IndexRecord &ir, int id) { return ir.supplierID < id; });
    if (it == indexTable.end() || it->supplierID != supplierID) {
        std::cout << "Supplier not found." << std::endl;
        return;
    }
    int suppRecNum = it->recordNumber;
    std::fstream mfile(MASTER_FILE, std::ios::binary | std::ios::in | std::ios::out);
    if (!mfile) {
        std::cerr << "Error opening master file." << std::endl;
        return;
    }
    mfile.seekg(suppRecNum * sizeof(Supplier));
    Supplier supp;
    mfile.read(reinterpret_cast<char*>(&supp), sizeof(Supplier));
    if (supp.valid == 0) {
        std::cout << "Supplier record is deleted." << std::endl;
        mfile.close();
        return;
    }
    // Prepare the supply record.
    Supply supplyRec;
    supplyRec.supplierID = supplierID;
    std::cout << "Enter Detail ID: ";
    std::cin >> supplyRec.detailID;
    std::cout << "Enter Price: ";
    std::cin >> supplyRec.price;
    std::cout << "Enter Quantity: ";
    std::cin >> supplyRec.quantity;
    supplyRec.nextSupply = supp.firstSupply; // New record becomes the first in the chain.
    supplyRec.valid = 1;
    
    int recNum;
    if (!slaveGarbage.empty()) {
        recNum = slaveGarbage.back();
        slaveGarbage.pop_back();
        std::fstream spfile(SLAVE_FILE, std::ios::binary | std::ios::in | std::ios::out);
        if (!spfile) {
            std::cerr << "Error opening slave file." << std::endl;
            mfile.close();
            return;
        }
        spfile.seekp(recNum * sizeof(Supply));
        spfile.write(reinterpret_cast<char*>(&supplyRec), sizeof(Supply));
        spfile.close();
    } else {
        std::ofstream spfile(SLAVE_FILE, std::ios::binary | std::ios::app);
        if (!spfile) {
            std::cerr << "Error opening slave file." << std::endl;
            mfile.close();
            return;
        }
        spfile.write(reinterpret_cast<char*>(&supplyRec), sizeof(Supply));
        spfile.close();
        std::ifstream in(SLAVE_FILE, std::ios::binary);
        in.seekg(0, std::ios::end);
        recNum = in.tellg() / sizeof(Supply) - 1;
        in.close();
    }
    // Update the supplier record: new supply becomes the first, increment supplyCount.
    supp.firstSupply = recNum;
    supp.supplyCount++;
    mfile.seekp(suppRecNum * sizeof(Supplier));
    mfile.write(reinterpret_cast<char*>(&supp), sizeof(Supplier));
    mfile.close();
    std::cout << "Supply record inserted." << std::endl;
}

// ===================== CALC FUNCTIONS =====================

// calc-m: Count valid supplier records.
void calcMaster() {
    std::ifstream mfile(MASTER_FILE, std::ios::binary);
    if (!mfile) {
        std::cerr << "Error opening master file." << std::endl;
        return;
    }
    int count = 0;
    Supplier supp;
    while (mfile.read(reinterpret_cast<char*>(&supp), sizeof(Supplier))) {
        if (supp.valid == 1)
            count++;
    }
    mfile.close();
    std::cout << "Total valid supplier records: " << count << std::endl;
}

// calc-s: Count valid supply records overall and display supplyCount for each supplier.
void calcSlave() {
    std::ifstream spfile(SLAVE_FILE, std::ios::binary);
    if (!spfile) {
        std::cerr << "Error opening slave file." << std::endl;
        return;
    }
    int total = 0;
    Supply supplyRec;
    while (spfile.read(reinterpret_cast<char*>(&supplyRec), sizeof(Supply))) {
        if (supplyRec.valid == 1)
            total++;
    }
    spfile.close();
    std::cout << "Total valid supply records: " << total << std::endl;
    
    std::cout << "Supply counts for each supplier (from master records):" << std::endl;
    std::ifstream mfile(MASTER_FILE, std::ios::binary);
    if (!mfile) {
        std::cerr << "Error opening master file." << std::endl;
        return;
    }
    Supplier supp;
    while (mfile.read(reinterpret_cast<char*>(&supp), sizeof(Supplier))) {
        if (supp.valid == 1) {
            std::cout << "Supplier ID " << supp.supplierID << ": " << supp.supplyCount << " supplies." << std::endl;
        }
    }
    mfile.close();
}

// ===================== UTILITY FUNCTIONS =====================

// ut-m: Print all master records (including service fields), index table and master garbage list.
void utMaster() {
    std::ifstream mfile(MASTER_FILE, std::ios::binary);
    if (!mfile) {
        std::cerr << "Error opening master file." << std::endl;
        return;
    }
    std::cout << "\n--- Master File Contents ---\n";
    int recNum = 0;
    Supplier supp;
    while (mfile.read(reinterpret_cast<char*>(&supp), sizeof(Supplier))) {
        std::cout << "Record " << recNum << ":\n";
        std::cout << "  Supplier ID: " << supp.supplierID << std::endl;
        std::cout << "  Surname: " << supp.surname << std::endl;
        std::cout << "  Status: " << supp.status << std::endl;
        std::cout << "  City: " << supp.city << std::endl;
        std::cout << "  First Supply Index: " << supp.firstSupply << std::endl;
        std::cout << "  Supply Count: " << supp.supplyCount << std::endl;
        std::cout << "  Valid: " << supp.valid << std::endl;
        recNum++;
    }
    mfile.close();
    std::cout << "--- End of Master File ---\n";
    std::cout << "Index Table:\n";
    for (auto &ir : indexTable)
        std::cout << "  Supplier ID: " << ir.supplierID << ", Record Number: " << ir.recordNumber << std::endl;
    std::cout << "Master Garbage List: ";
    for (auto &g : masterGarbage)
        std::cout << g << " ";
    std::cout << "\n";
}

// ut-s: Print all slave records (including service fields) and slave garbage list.
void utSlave() {
    std::ifstream spfile(SLAVE_FILE, std::ios::binary);
    if (!spfile) {
        std::cerr << "Error opening slave file." << std::endl;
        return;
    }
    std::cout << "\n--- Slave File Contents ---\n";
    int recNum = 0;
    Supply supplyRec;
    while (spfile.read(reinterpret_cast<char*>(&supplyRec), sizeof(Supply))) {
        std::cout << "Record " << recNum << ":\n";
        std::cout << "  Supplier ID: " << supplyRec.supplierID << std::endl;
        std::cout << "  Detail ID: " << supplyRec.detailID << std::endl;
        std::cout << "  Price: " << supplyRec.price << std::endl;
        std::cout << "  Quantity: " << supplyRec.quantity << std::endl;
        std::cout << "  Next Supply Index: " << supplyRec.nextSupply << std::endl;
        std::cout << "  Valid: " << supplyRec.valid << std::endl;
        recNum++;
    }
    spfile.close();
    std::cout << "--- End of Slave File ---\n";
    std::cout << "Slave Garbage List: ";
    for (auto &g : slaveGarbage)
        std::cout << g << " ";
    std::cout << "\n";
}

// ===================== MAIN FUNCTION =====================
int main() {
    // Load index table and garbage zones from files (if they exist)
    loadIndexTable();
    loadMasterGarbage();
    loadSlaveGarbage();
    
    std::string command;
    do {
        std::cout << "\nEnter command (get-m, get-s, del-m, del-s, update-m, update-s, insert-m, insert-s, calc-m, calc-s, ut-m, ut-s, exit): ";
        std::cin >> command;
        if (command == "get-m")      getMaster();
        else if (command == "get-s") getSlave();
        else if (command == "del-m") delMaster();
        else if (command == "del-s") delSlave();
        else if (command == "update-m") updateMaster();
        else if (command == "update-s") updateSlave();
        else if (command == "insert-m") insertMaster();
        else if (command == "insert-s") insertSlave();
        else if (command == "calc-m")   calcMaster();
        else if (command == "calc-s")   calcSlave();
        else if (command == "ut-m")     utMaster();
        else if (command == "ut-s")     utSlave();
        else if (command == "exit") break;
        else std::cout << "Unknown command." << std::endl;
    } while(command != "exit");
    
    // Before exiting, save index table and garbage zones to files
    saveIndexTable();
    saveMasterGarbage();
    saveSlaveGarbage();
    
    return 0;
}
