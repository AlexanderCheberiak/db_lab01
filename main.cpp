#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>

// Назви файлів (бінарні файли з записами фіксованої довжини)
const char* MASTER_FILE = "B.fl";         
const char* SLAVE_FILE  = "BK.fl";            
const char* INDEX_FILE  = "B.ind";            
const char* MASTER_GARBAGE_FILE = "B.garbage";  
const char* SLAVE_GARBAGE_FILE  = "BK.garbage";  

// Master
struct Buyer {
    int phone;
    char name[31];
    char address[31];
    int firstBook;   // номер запису першої книги у BK.fl (-1 якщо немає)
    int bookCount;
    int valid;       // 1 = дійсний, 0 = видалений
};

// Slave
struct Book {
    int phone;
    int ISBN;
    char name[31];
    char author[31];
    double price;
    int nextBook;    // наступний запис у BK.fl (-1 якщо немає)
    int valid;       // 1 = дійсний, 0 = видалений
};

// структура для B.fl
struct IndexRecord {
    int phone;
    int recordNumber;  
};

// вектори для зберігання індексів та зон сміття
std::vector<IndexRecord> indexTable;
std::vector<int> masterGarbage; 
std::vector<int> slaveGarbage;  

//робота з індексами та сміттям
void loadIndexTable() {
    indexTable.clear();
    std::ifstream in(INDEX_FILE, std::ios::binary);
    if (!in) {
        // Якщо файл з індексною таблицею не існує, створюємо.
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

//get-функції

// get-m: прочитати master за телефоном та вивести всі поля
void getMaster() {
    int phone;
    std::cout << "Enter Phone: ";
    std::cin >> phone;
    
    // бінарний пошук в індексній таблиці(відсортованій за телефоном)
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

// get-s: Прочитати slave запис (книгу) за телефоном та ISBN пройшовши по зв'язаному списку
void getSlave() {
    int phone, ISBN;
    std::cout << "Enter Phone: ";
    std::cin >> phone;
    std::cout << "Enter ISBN: ";
    std::cin >> ISBN;
    
    // Знайти запис покупця в індексній таблиці
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

//Функції для видалення записів

// del-m: Видалити master запис (покупця) за телефоном та всі його записи книг
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
    // видалити всі підлеглі записи книг
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
    // позначити запис покупця як видалений
    buyer.valid = 0;
    mfile.seekp(buyerRecNum * sizeof(Buyer));
    mfile.write(reinterpret_cast<char*>(&buyer), sizeof(Buyer));
    mfile.close();
    masterGarbage.push_back(buyerRecNum);
    // видалити з індексної таблиці
    indexTable.erase(it);
    std::cout << "Buyer and their books have been deleted." << std::endl;
}

// del-s: видалити підлеглий запис книги (за телефоном та ISBN).
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
    // пошук книги у зв'язаному списку
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
            // якщо перший запис у списку
            if (prevIndex == -1) {
                buyer.firstBook = bookRec.nextBook;
            } else {
                // оновити nextBook попереднього запису
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

//Функції для оновлення записів 

// update-m: оновити не ключову поле (ім'я, адреса) запису покупця.
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

// update-s: Оновити не ключову поле (назва, автор, ціна) запису книги.
void updateSlave() {
    int phone, ISBN;
    std::cout << "Enter Buyer Phone for book update: ";
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

// Функції для додавання нових записів

// insert-m: додати новий запис покупця в B.fl, за можливості використовуючи зону сміття.
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
    // Використати зону сміття, якщо вона не пуста
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
    // Оновити індексну таблицю
    IndexRecord ir;
    ir.phone = buyer.phone;
    ir.recordNumber = recNum;
    indexTable.push_back(ir);
    std::sort(indexTable.begin(), indexTable.end(), [](const IndexRecord &a, const IndexRecord &b) {
        return a.phone < b.phone;
    });
    std::cout << "Buyer record inserted." << std::endl;
}

// insert-s: Додати новий запис книги у BK.fl та поєднати його з першим записом у списку покупця.
void insertSlave() {
    int phone;
    std::cout << "Enter Buyer Phone: ";
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
        std::cout << "Buyer record is deleted." << std::endl;
        mfile.close();
        return;
    }

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
    bookRec.nextBook = buyer.firstBook; // Новий запис стає першим
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
    // Оновити запис покупця: нова книга стає першою, збільшуємо bookCount.
    buyer.firstBook = recNum;
    buyer.bookCount++;
    mfile.seekp(buyerRecNum * sizeof(Buyer));
    mfile.write(reinterpret_cast<char*>(&buyer), sizeof(Buyer));
    mfile.close();
    std::cout << "Book record inserted." << std::endl;
}

// Функції для підрахунку кількості записів

// calc-m: Підрахувати кількість дійсних записів покупців.
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

// calc-s: Підрахувати кількість дійсних записів книг та вивести кількість книг для кожного покупця.
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

// Функції для виведення вмісту файлів та зон сміття

// ut-m: Вивести всі master записи (включаючи службові поля), індексну таблицю та список сміття.
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

// ut-s: Вивести всі slave записи (включаючи службові поля) та список сміття.
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

int main() {
    // Завантаження індексних таблиць та зон сміття перед початком роботи
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
    
    // Збереження індексних таблиць та зон сміття перед виходом
    saveIndexTable();
    saveMasterGarbage();
    saveSlaveGarbage();
    
    return 0;
}