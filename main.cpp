#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>

// File names (binary files with fixed-length records)
const char* MASTER_FILE = "B.fl";             // Master file (buyers)
const char* SLAVE_FILE  = "BK.fl";            // Slave file (books)
const char* INDEX_FILE  = "B.ind";            // Index table for B.fl
const char* MASTER_GARBAGE_FILE = "B.garbage";  // Garbage zone for master file
const char* SLAVE_GARBAGE_FILE  = "BK.garbage";  // Garbage zone for slave file

// Structure for a buyer (master record)
// Fields:
//  phone (key), name, address,
//  firstBook (record number in BK.fl for first book, -1 if none),
//  bookCount (number of books),
//  valid (1 - record exists, 0 - logically deleted)
struct Buyer {
    int phone;
    char name[31];
    char address[31];
    int firstBook;   // Number of the first book record in BK.fl (-1 if none)
    int bookCount;
    int valid;       // 1 = exists, 0 = deleted
};

// Structure for a book record (slave record)
// Fields:
//  phone (foreign key), ISBN, name, author, price,
//  nextBook (number of the next book record in the chain, -1 if none),
//  valid (1 - exists, 0 - logically deleted)
struct Book {
    int phone;
    int ISBN;
    char name[31];
    char author[31];
    double price;
    int nextBook;    // Next record in BK.fl (-1 if none)
    int valid;       // 1 = exists, 0 = deleted
};

// Structure for an index table record (for B.fl)
struct IndexRecord {
    int phone;
    int recordNumber;  // Record number in B.fl
};

std::vector<IndexRecord> indexTable;
std::vector<int> masterGarbage; // Record numbers of logically deleted Buyer records in B.fl
std::vector<int> slaveGarbage;  // Record numbers of logically deleted Book records in BK.fl

// ===================== INDEX AND GARBAGE HANDLING =====================
void loadIndexTable() {
    indexTable.clear();
    std::ifstream in(INDEX_FILE, std::ios::binary);
    if (!in) {
        // If the index table file does not exist, scan B.fl to build it.
        std::ifstream master(MASTER_FILE, std::ios::binary);
        if (!master) return;
        Buyer buyer;
        int recNum = 0;
        while (master.read(reinterpret_cast<char*>(&buyer), sizeof(Buyer))) {
            if (buyer.valid == 1) {
                IndexRecord ir;
                ir.phone = buyer.phone;
                ir.recordNumber = recNum;
                indexTable.push_back(ir);
            }
            recNum++;
        }
        master.close();
        std::sort(indexTable.begin(), indexTable.end(), [](const IndexRecord &a, const IndexRecord &b) {
            return a.phone < b.phone;
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

// get-m: Read master record by phone and display its fields.
void getMaster() {
    int phone;
    std::cout << "Enter Phone: ";
    std::cin >> phone;
    
    // Binary search in the index table (index table is sorted by phone)
    auto it = std::lower_bound(indexTable.begin(), indexTable.end(), phone,
        [](const IndexRecord &ir, int id) { return ir.phone < id; });
    if (it == indexTable.end() || it->phone != phone) {
        std::cout << "Buyer not found." << std::endl;
        return;
    }
    int recNum = it->recordNumber;
    std::fstream file(MASTER_FILE, std::ios::binary | std::ios::in);
    if (!file) {
        std::cerr << "Error opening master file." << std::endl;
        return;
    }
    file.seekg(recNum * sizeof(Buyer));
    Buyer buyer;
    file.read(reinterpret_cast<char*>(&buyer), sizeof(Buyer));
    file.close();
    
    if (buyer.valid == 0) {
        std::cout << "Buyer record is deleted." << std::endl;
        return;
    }
    std::cout << "\nBuyer Record:" << std::endl;
    std::cout << "Phone: " << buyer.phone << std::endl;
    std::cout << "Name: " << buyer.name << std::endl;
    std::cout << "Address: " << buyer.address << std::endl;
    std::cout << "First Book Index: " << buyer.firstBook << std::endl;
    std::cout << "Book Count: " << buyer.bookCount << std::endl;
}

// get-s: Read slave record (book) by phone and ISBN by traversing the linked list.
void getSlave() {
    int phone, ISBN;
    std::cout << "Enter Phone: ";
    std::cin >> phone;
    std::cout << "Enter ISBN: ";
    std::cin >> ISBN;
    
    // Find the buyer via the index table
    auto it = std::lower_bound(indexTable.begin(), indexTable.end(), phone,
        [](const IndexRecord &ir, int id) { return ir.phone < id; });
    if (it == indexTable.end() || it->phone != phone) {
        std::cout << "Buyer not found." << std::endl;
        return;
    }
    int buyerRecNum = it->recordNumber;
    std::fstream bfile(MASTER_FILE, std::ios::binary | std::ios::in);
    if (!bfile) {
        std::cerr << "Error opening master file." << std::endl;
        return;
    }
    bfile.seekg(buyerRecNum * sizeof(Buyer));
    Buyer buyer;
    bfile.read(reinterpret_cast<char*>(&buyer), sizeof(Buyer));
    bfile.close();
    if (buyer.valid == 0) {
        std::cout << "Buyer record is deleted." << std::endl;
        return;
    }
    
    int bookIndex = buyer.firstBook;
    std::fstream bkfile(SLAVE_FILE, std::ios::binary | std::ios::in);
    if (!bkfile) {
        std::cerr << "Error opening slave file." << std::endl;
        return;
    }
    bool found = false;
    while (bookIndex != -1) {
        bkfile.seekg(bookIndex * sizeof(Book));
        Book bookRec;
        bkfile.read(reinterpret_cast<char*>(&bookRec), sizeof(Book));
        if (bookRec.valid == 1 && bookRec.ISBN == ISBN) {
            std::cout << "\nBook Record:" << std::endl;
            std::cout << "Phone: " << bookRec.phone << std::endl;
            std::cout << "ISBN: " << bookRec.ISBN << std::endl;
            std::cout << "Name: " << bookRec.name << std::endl;
            std::cout << "Author: " << bookRec.author << std::endl;
            std::cout << "Price: " << bookRec.price << std::endl;
            std::cout << "Next Book Index: " << bookRec.nextBook << std::endl;
            found = true;
            break;
        }
        bookIndex = bookRec.nextBook;
    }
    bkfile.close();
    if (!found)
        std::cout << "Book record not found." << std::endl;
}

// ===================== DELETE FUNCTIONS =====================

// del-m: Delete a master record (buyer) by phone and all its subordinate book records.
void delMaster() {
    int phone;
    std::cout << "Enter Phone to delete: ";
    std::cin >> phone;
    
    auto it = std::lower_bound(indexTable.begin(), indexTable.end(), phone,
        [](const IndexRecord &ir, int id) { return ir.phone < id; });
    if (it == indexTable.end() || it->phone != phone) {
        std::cout << "Buyer not found." << std::endl;
        return;
    }
    int buyerRecNum = it->recordNumber;
    std::fstream mfile(MASTER_FILE, std::ios::binary | std::ios::in | std::ios::out);
    if (!mfile) {
        std::cerr << "Error opening master file." << std::endl;
        return;
    }
    mfile.seekg(buyerRecNum * sizeof(Buyer));
    Buyer buyer;
    mfile.read(reinterpret_cast<char*>(&buyer), sizeof(Buyer));
    if (buyer.valid == 0) {
        std::cout << "Buyer already deleted." << std::endl;
        mfile.close();
        return;
    }
    // Delete all subordinate book records
    int bookIndex = buyer.firstBook;
    std::fstream bkfile(SLAVE_FILE, std::ios::binary | std::ios::in | std::ios::out);
    if (!bkfile) {
        std::cerr << "Error opening slave file." << std::endl;
        mfile.close();
        return;
    }
    while (bookIndex != -1) {
        bkfile.seekg(bookIndex * sizeof(Book));
        Book bookRec;
        bkfile.read(reinterpret_cast<char*>(&bookRec), sizeof(Book));
        if (bookRec.valid == 1) {
            bookRec.valid = 0;
            bkfile.seekp(bookIndex * sizeof(Book));
            bkfile.write(reinterpret_cast<char*>(&bookRec), sizeof(Book));
            slaveGarbage.push_back(bookIndex);
        }
        bookIndex = bookRec.nextBook;
    }
    bkfile.close();
    // Mark buyer record as deleted
    buyer.valid = 0;
    mfile.seekp(buyerRecNum * sizeof(Buyer));
    mfile.write(reinterpret_cast<char*>(&buyer), sizeof(Buyer));
    mfile.close();
    masterGarbage.push_back(buyerRecNum);
    // Remove from index table
    indexTable.erase(it);
    std::cout << "Buyer and their books have been deleted." << std::endl;
}

// del-s: Delete a subordinate book record (by phone and ISBN).
void delSlave() {
    int phone, ISBN;
    std::cout << "Enter Phone for book deletion: ";
    std::cin >> phone;
    std::cout << "Enter ISBN of the book to delete: ";
    std::cin >> ISBN;
    
    auto it = std::lower_bound(indexTable.begin(), indexTable.end(), phone,
        [](const IndexRecord &ir, int id) { return ir.phone < id; });
    if (it == indexTable.end() || it->phone != phone) {
        std::cout << "Buyer not found." << std::endl;
        return;
    }
    int buyerRecNum = it->recordNumber;
    std::fstream mfile(MASTER_FILE, std::ios::binary | std::ios::in | std::ios::out);
    if (!mfile) {
        std::cerr << "Error opening master file." << std::endl;
        return;
    }
    mfile.seekg(buyerRecNum * sizeof(Buyer));
    Buyer buyer;
    mfile.read(reinterpret_cast<char*>(&buyer), sizeof(Buyer));
    if (buyer.valid == 0) {
        std::cout << "Buyer record is deleted." << std::endl;
        mfile.close();
        return;
    }
    // Search for the book in the linked list
    int currentIndex = buyer.firstBook;
    int prevIndex = -1;
    bool found = false;
    std::fstream bkfile(SLAVE_FILE, std::ios::binary | std::ios::in | std::ios::out);
    if (!bkfile) {
        std::cerr << "Error opening slave file." << std::endl;
        mfile.close();
        return;
    }
    while (currentIndex != -1) {
        bkfile.seekg(currentIndex * sizeof(Book));
        Book bookRec;
        bkfile.read(reinterpret_cast<char*>(&bookRec), sizeof(Book));
        if (bookRec.valid == 1 && bookRec.ISBN == ISBN) {
            found = true;
            // If this is the first record in the chain
            if (prevIndex == -1) {
                buyer.firstBook = bookRec.nextBook;
            } else {
                // Update nextBook of the previous record
                bkfile.seekg(prevIndex * sizeof(Book));
                Book prevRec;
                bkfile.read(reinterpret_cast<char*>(&prevRec), sizeof(Book));
                prevRec.nextBook = bookRec.nextBook;
                bkfile.seekp(prevIndex * sizeof(Book));
                bkfile.write(reinterpret_cast<char*>(&prevRec), sizeof(Book));
            }
            bookRec.valid = 0;
            bkfile.seekp(currentIndex * sizeof(Book));
            bkfile.write(reinterpret_cast<char*>(&bookRec), sizeof(Book));
            slaveGarbage.push_back(currentIndex);
            buyer.bookCount--;
            break;
        }
        prevIndex = currentIndex;
        currentIndex = bookRec.nextBook;
    }
    bkfile.close();
    mfile.seekp(buyerRecNum * sizeof(Buyer));
    mfile.write(reinterpret_cast<char*>(&buyer), sizeof(Buyer));
    mfile.close();
    if (!found)
        std::cout << "Book record not found." << std::endl;
    else
        std::cout << "Book record deleted." << std::endl;
}

// ===================== UPDATE FUNCTIONS =====================

// update-m: Update a non-key field (name, address) of a buyer record.
void updateMaster() {
    int phone;
    std::cout << "Enter Phone to update: ";
    std::cin >> phone;
    
    auto it = std::lower_bound(indexTable.begin(), indexTable.end(), phone,
        [](const IndexRecord &ir, int id) { return ir.phone < id; });
    if (it == indexTable.end() || it->phone != phone) {
        std::cout << "Buyer not found." << std::endl;
        return;
    }
    int recNum = it->recordNumber;
    std::fstream mfile(MASTER_FILE, std::ios::binary | std::ios::in | std::ios::out);
    if (!mfile) {
        std::cerr << "Error opening master file." << std::endl;
        return;
    }
    mfile.seekg(recNum * sizeof(Buyer));
    Buyer buyer;
    mfile.read(reinterpret_cast<char*>(&buyer), sizeof(Buyer));
    if (buyer.valid == 0) {
        std::cout << "Buyer record is deleted." << std::endl;
        mfile.close();
        return;
    }
    int choice;
    std::cout << "Select field to update:\n1. Name\n2. Address\nChoice: ";
    std::cin >> choice;
    switch(choice) {
        case 1:
            std::cout << "Enter new Name: ";
            std::cin >> buyer.name;
            break;
        case 2:
            std::cout << "Enter new Address: ";
            std::cin >> buyer.address;
            break;
        default:
            std::cout << "Invalid choice." << std::endl;
            mfile.close();
            return;
    }
    mfile.seekp(recNum * sizeof(Buyer));
    mfile.write(reinterpret_cast<char*>(&buyer), sizeof(Buyer));
    mfile.close();
    std::cout << "Buyer record updated." << std::endl;
}

// update-s: Update a non-key field (name, author, price) of a book record.
void updateSlave() {
    int phone, ISBN;
    std::cout << "Enter Phone for book update: ";
    std::cin >> phone;
    std::cout << "Enter ISBN of book to update: ";
    std::cin >> ISBN;
    
    auto it = std::lower_bound(indexTable.begin(), indexTable.end(), phone,
        [](const IndexRecord &ir, int id) { return ir.phone < id; });
    if (it == indexTable.end() || it->phone != phone) {
        std::cout << "Buyer not found." << std::endl;
        return;
    }
    int buyerRecNum = it->recordNumber;
    std::fstream mfile(MASTER_FILE, std::ios::binary | std::ios::in);
    if (!mfile) {
        std::cerr << "Error opening master file." << std::endl;
        return;
    }
    mfile.seekg(buyerRecNum * sizeof(Buyer));
    Buyer buyer;
    mfile.read(reinterpret_cast<char*>(&buyer), sizeof(Buyer));
    mfile.close();
    if (buyer.valid == 0) {
        std::cout << "Buyer record is deleted." << std::endl;
        return;
    }
    int currentIndex = buyer.firstBook;
    int targetIndex = -1;
    std::fstream bkfile(SLAVE_FILE, std::ios::binary | std::ios::in | std::ios::out);
    if (!bkfile) {
        std::cerr << "Error opening slave file." << std::endl;
        return;
    }
    Book bookRec;
    bool found = false;
    while (currentIndex != -1) {
        bkfile.seekg(currentIndex * sizeof(Book));
        bkfile.read(reinterpret_cast<char*>(&bookRec), sizeof(Book));
        if (bookRec.valid == 1 && bookRec.ISBN == ISBN) {
            found = true;
            targetIndex = currentIndex;
            break;
        }
        currentIndex = bookRec.nextBook;
    }
    if (!found) {
        std::cout << "Book record not found." << std::endl;
        bkfile.close();
        return;
    }
    int choice;
    std::cout << "Select field to update:\n1. Name\n2. Author\n3. Price\nChoice: ";
    std::cin >> choice;
    switch(choice) {
        case 1:
            std::cout << "Enter new Name: ";
            std::cin >> bookRec.name;
            break;
        case 2:
            std::cout << "Enter new Author: ";
            std::cin >> bookRec.author;
            break;
        case 3:
            std::cout << "Enter new Price: ";
            std::cin >> bookRec.price;
            break;
        default:
            std::cout << "Invalid choice." << std::endl;
            bkfile.close();
            return;
    }
    bkfile.seekp(targetIndex * sizeof(Book));
    bkfile.write(reinterpret_cast<char*>(&bookRec), sizeof(Book));
    bkfile.close();
    std::cout << "Book record updated." << std::endl;
}

// ===================== INSERT FUNCTIONS =====================

// insert-m: Insert a new buyer record into B.fl, using the master garbage zone if available.
void insertMaster() {
    Buyer buyer;
    std::cout << "Enter Phone: ";
    std::cin >> buyer.phone;
    std::cout << "Enter Name: ";
    std::cin >> buyer.name;
    std::cout << "Enter Address: ";
    std::cin >> buyer.address;
    buyer.firstBook = -1;
    buyer.bookCount = 0;
    buyer.valid = 1;
    
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
        mfile.seekp(recNum * sizeof(Buyer));
        mfile.write(reinterpret_cast<char*>(&buyer), sizeof(Buyer));
        mfile.close();
    } else {
        std::ofstream mfile(MASTER_FILE, std::ios::binary | std::ios::app);
        if (!mfile) {
            std::cerr << "Error opening master file." << std::endl;
            return;
        }
        mfile.write(reinterpret_cast<char*>(&buyer), sizeof(Buyer));
        mfile.close();
        std::ifstream in(MASTER_FILE, std::ios::binary);
        in.seekg(0, std::ios::end);
        recNum = in.tellg() / sizeof(Buyer) - 1;
        in.close();
    }
    // Update the index table.
    IndexRecord ir;
    ir.phone = buyer.phone;
    ir.recordNumber = recNum;
    indexTable.push_back(ir);
    std::sort(indexTable.begin(), indexTable.end(), [](const IndexRecord &a, const IndexRecord &b) {
        return a.phone < b.phone;
    });
    std::cout << "Buyer record inserted." << std::endl;
}

// insert-s: Insert a new book record into BK.fl and link it as the first record in the buyer's chain.
void insertSlave() {
    int phone;
    std::cout << "Enter Phone for the book: ";
    std::cin >> phone;
    // Find the buyer.
    auto it = std::lower_bound(indexTable.begin(), indexTable.end(), phone,
        [](const IndexRecord &ir, int id) { return ir.phone < id; });
    if (it == indexTable.end() || it->phone != phone) {
        std::cout << "Buyer not found." << std::endl;
        return;
    }
    int buyerRecNum = it->recordNumber;
    std::fstream mfile(MASTER_FILE, std::ios::binary | std::ios::in | std::ios::out);
    if (!mfile) {
        std::cerr << "Error opening master file." << std::endl;
        return;
    }
    mfile.seekg(buyerRecNum * sizeof(Buyer));
    Buyer buyer;
    mfile.read(reinterpret_cast<char*>(&buyer), sizeof(Buyer));
    if (buyer.valid == 0) {
        std::cout << "Buyer record is deleted." << std::endl;
        mfile.close();
        return;
    }
    // Prepare the book record.
    Book bookRec;
    bookRec.phone = phone;
    std::cout << "Enter ISBN: ";
    std::cin >> bookRec.ISBN;
    std::cout << "Enter Name: ";
    std::cin >> bookRec.name;
    std::cout << "Enter Author: ";
    std::cin >> bookRec.author;
    std::cout << "Enter Price: ";
    std::cin >> bookRec.price;
    bookRec.nextBook = buyer.firstBook; // New record becomes the first in the chain.
    bookRec.valid = 1;
    
    int recNum;
    if (!slaveGarbage.empty()) {
        recNum = slaveGarbage.back();
        slaveGarbage.pop_back();
        std::fstream bkfile(SLAVE_FILE, std::ios::binary | std::ios::in | std::ios::out);
        if (!bkfile) {
            std::cerr << "Error opening slave file." << std::endl;
            mfile.close();
            return;
        }
        bkfile.seekp(recNum * sizeof(Book));
        bkfile.write(reinterpret_cast<char*>(&bookRec), sizeof(Book));
        bkfile.close();
    } else {
        std::ofstream bkfile(SLAVE_FILE, std::ios::binary | std::ios::app);
        if (!bkfile) {
            std::cerr << "Error opening slave file." << std::endl;
            mfile.close();
            return;
        }
        bkfile.write(reinterpret_cast<char*>(&bookRec), sizeof(Book));
        bkfile.close();
        std::ifstream in(SLAVE_FILE, std::ios::binary);
        in.seekg(0, std::ios::end);
        recNum = in.tellg() / sizeof(Book) - 1;
        in.close();
    }
    // Update the buyer record: new book becomes the first, increment bookCount.
    buyer.firstBook = recNum;
    buyer.bookCount++;
    mfile.seekp(buyerRecNum * sizeof(Buyer));
    mfile.write(reinterpret_cast<char*>(&buyer), sizeof(Buyer));
    mfile.close();
    std::cout << "Book record inserted." << std::endl;
}

// ===================== CALC FUNCTIONS =====================

// calc-m: Count valid buyer records.
void calcMaster() {
    std::ifstream mfile(MASTER_FILE, std::ios::binary);
    if (!mfile) {
        std::cerr << "Error opening master file." << std::endl;
        return;
    }
    int count = 0;
    Buyer buyer;
    while (mfile.read(reinterpret_cast<char*>(&buyer), sizeof(Buyer))) {
        if (buyer.valid == 1)
            count++;
    }
    mfile.close();
    std::cout << "Total valid buyer records: " << count << std::endl;
}

// calc-s: Count valid book records overall and display bookCount for each buyer.
void calcSlave() {
    std::ifstream bkfile(SLAVE_FILE, std::ios::binary);
    if (!bkfile) {
        std::cerr << "Error opening slave file." << std::endl;
        return;
    }
    int total = 0;
    Book bookRec;
    while (bkfile.read(reinterpret_cast<char*>(&bookRec), sizeof(Book))) {
        if (bookRec.valid == 1)
            total++;
    }
    bkfile.close();
    std::cout << "Total valid book records: " << total << std::endl;
    
    std::cout << "Book counts for each buyer (from master records):" << std::endl;
    std::ifstream mfile(MASTER_FILE, std::ios::binary);
    if (!mfile) {
        std::cerr << "Error opening master file." << std::endl;
        return;
    }
    Buyer buyer;
    while (mfile.read(reinterpret_cast<char*>(&buyer), sizeof(Buyer))) {
        if (buyer.valid == 1) {
            std::cout << "Phone " << buyer.phone << ": " << buyer.bookCount << " books." << std::endl;
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
    Buyer buyer;
    while (mfile.read(reinterpret_cast<char*>(&buyer), sizeof(Buyer))) {
        std::cout << "Record " << recNum << ":\n";
        std::cout << "  Phone: " << buyer.phone << std::endl;
        std::cout << "  Name: " << buyer.name << std::endl;
        std::cout << "  Address: " << buyer.address << std::endl;
        std::cout << "  First Book Index: " << buyer.firstBook << std::endl;
        std::cout << "  Book Count: " << buyer.bookCount << std::endl;
        std::cout << "  Valid: " << buyer.valid << std::endl;
        recNum++;
    }
    mfile.close();
    std::cout << "--- End of Master File ---\n";
    std::cout << "Index Table:\n";
    for (auto &ir : indexTable)
        std::cout << "  Phone: " << ir.phone << ", Record Number: " << ir.recordNumber << std::endl;
    std::cout << "Master Garbage List: ";
    for (auto &g : masterGarbage)
        std::cout << g << " ";
    std::cout << "\n";
}

// ut-s: Print all slave records (including service fields) and slave garbage list.
void utSlave() {
    std::ifstream bkfile(SLAVE_FILE, std::ios::binary);
    if (!bkfile) {
        std::cerr << "Error opening slave file." << std::endl;
        return;
    }
    std::cout << "\n--- Slave File Contents ---\n";
    int recNum = 0;
    Book bookRec;
    while (bkfile.read(reinterpret_cast<char*>(&bookRec), sizeof(Book))) {
        std::cout << "Record " << recNum << ":\n";
        std::cout << "  Phone: " << bookRec.phone << std::endl;
        std::cout << "  ISBN: " << bookRec.ISBN << std::endl;
        std::cout << "  Name: " << bookRec.name << std::endl;
        std::cout << "  Author: " << bookRec.author << std::endl;
        std::cout << "  Price: " << bookRec.price << std::endl;
        std::cout << "  Next Book Index: " << bookRec.nextBook << std::endl;
        std::cout << "  Valid: " << bookRec.valid << std::endl;
        recNum++;
    }
    bkfile.close();
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