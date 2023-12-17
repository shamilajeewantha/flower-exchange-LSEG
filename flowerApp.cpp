#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <queue>
#include <stack>
#include <iomanip>
#include <chrono>
#include <unordered_set>
#include <unordered_map>

#define FLOWER_TYPES std::unordered_set<std::string>({"Rose", "Lavender", "Lotus", "Tulip", "Orchid"})

using namespace std;

///////////////////////////////////////////// Data Structures //////////////////////////////////////////////////////////////

// Stores all the order details coming from the csv
struct OrderData
{
    string OrderID;
    string clientOrderID;
    string instrument;
    int side;
    int quantity;
    double price;
    string status;
    string reason;
    int priorityNumber;

    // Overloaded '<' operator for comparing OrderData
    bool operator<(const OrderData &other) const
    {

        if (price != other.price)
        {
            if (side == 1)
                return price < other.price;
            else if (side == 2)
                return price > other.price;
        }
        // If prices are equal, compare based on priorityNumber
        return priorityNumber > other.priorityNumber;
    }
};

// one order book is maintained per flower type
struct OrderBook
{
    string Flower;
    std::unordered_map<double, int> orders_count_for_price_side1;
    std::unordered_map<double, int> orders_count_for_price_side2;
    std::priority_queue<OrderData> side1;
    std::priority_queue<OrderData> side2;
};

///////////////////////////////////////////// Global variables //////////////////////////////////////////////////////////////

// Creating the Execution Report file
ofstream reportFile("execution_rep.csv");

// used to store all 5 order books created
std::unordered_map<std::string, OrderBook> orderBooks;

// Declare global variables for start and end timestamps
std::chrono::high_resolution_clock::time_point start_time;
std::chrono::high_resolution_clock::time_point end_time;

////////////////////////////////////////////////// Functions //////////////////////////////////////////////////////////////

// Initialize 5 order books based on flower types
void initializeOrderBooks()
{
    for (const auto &flower : FLOWER_TYPES)
    {
        OrderBook book;
        book.Flower = flower;
        orderBooks[flower] = book;
    }
}

// Function to print the contents of priority queues in order books
void printPriorityQueue(const std::priority_queue<OrderData> &pq)

{
    std::priority_queue<OrderData> tempQueue = pq; // Copy the queue to preserve the original
    while (!tempQueue.empty())
    {
        OrderData order = tempQueue.top();
        std::cout << "OrderID: " << order.OrderID << ", ClientOrderID: " << order.clientOrderID
                  << ", Instrument: " << order.instrument << ", Side: " << order.side
                  << ", Quantity: " << order.quantity << ", Price: " << order.price
                  << ", Status: " << order.status << ", Reason: " << order.reason
                  << ", Priority Number: " << order.priorityNumber << std::endl;
        tempQueue.pop();
    }
}

// Used to save order books sides to csv files
void savePriorityQueueToFile(std::priority_queue<OrderData> &pq, const std::string &filename)
{
    ofstream file(filename);
    if (!file.is_open())
    {
        cerr << "Unable to create file: " << filename << endl;
        return;
    }

    while (!pq.empty())
    {
        OrderData order = pq.top();
        file << order.OrderID << ","
             << order.clientOrderID << ","
             << order.instrument << ","
             << order.side << ","
             << order.quantity << ","
             << order.price << ","
             << order.status << ","
             << order.reason << ","
             << order.priorityNumber << "\n";
        pq.pop();
    }

    file.close();
}

// Validate all the orders before processing
bool validOrder(OrderData &order)
{
    if (order.side != 1 && order.side != 2)
    {
        order.reason = "Invalid side";
        return false;
    }
    if (order.quantity % 10 != 0 || order.quantity < 10 || order.quantity > 1000)
    {
        order.reason = "Invalid quantity";
        return false;
    }
    if (order.price <= 0)
    {
        order.reason = "Invalid price";
        return false;
    }
    if (FLOWER_TYPES.find(order.instrument) == FLOWER_TYPES.end())
    {
        order.reason = "Invalid flower type";
        return false;
    }
    order.reason = "Valid Order";
    return true;
}

// Used to save entries to execution_rep.csv
void writeExecution(OrderData &order)
{
    end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end_time - start_time;

    // Writing the processed data and additional columns to the report
    reportFile << order.OrderID << ","
               << order.clientOrderID << ","
               << order.instrument << ","
               << order.side << ","
               << order.status << ","
               << order.quantity << ","
               << std::fixed << std::setprecision(2) << order.price << "," // assuming the precision needed in the execution report is 2 decimal points as in examples. But for internal execution the real precise number is considered.
               << order.reason << ","
               << to_string(duration.count()) << ","
               << order.priorityNumber << "\n";
}

// Function to process data and create Execution Report
void processOrder(OrderData &order)
{
    // start time for valid transactions
    start_time = std::chrono::high_resolution_clock::now();

    if (order.side == 1)
    {
        cout << "buy order : " << order.clientOrderID << " " << order.price << endl;
        // if side 2 pq empty no need to execute add new to side 1
        if (orderBooks[order.instrument].side2.empty())
        {
            order.status = "New";
            order.priorityNumber = ++orderBooks[order.instrument].orders_count_for_price_side1[order.price];
            orderBooks[order.instrument].side1.push(order);
            cout << "Side 2 empty : " << order.status << "\n\n";
            writeExecution(order);
        }
        // side2 has a top and not empty
        // top sell price higher than buy order can't execute; add buy order
        else if (orderBooks[order.instrument].side2.top().price > order.price)
        {
            order.status = "New";
            order.priorityNumber = ++orderBooks[order.instrument].orders_count_for_price_side1[order.price];
            orderBooks[order.instrument].side1.push(order);
            cout << "Side 2 sell price higher : top sell price : " << orderBooks[order.instrument].side2.top().price << " " << order.status << "\n\n";
            writeExecution(order);
        }
        // execution can happen
        else
        {
            if (orderBooks[order.instrument].side2.top().quantity == order.quantity)
            {
                OrderData topOrder = orderBooks[order.instrument].side2.top();
                topOrder.status = "Fill";
                orderBooks[order.instrument].side2.pop();

                order.price = topOrder.price;
                order.status = "Fill";
                order.priorityNumber = -1; // this order doesn't enter orderbooks

                writeExecution(order);
                writeExecution(topOrder);
                cout << "Fill Fill : " << topOrder.clientOrderID << "\n\n";
            }
            else if (orderBooks[order.instrument].side2.top().quantity > order.quantity)
            {
                OrderData topOrder = orderBooks[order.instrument].side2.top();
                topOrder.quantity -= order.quantity;
                topOrder.status = "Pfill";
                orderBooks[order.instrument].side2.pop();
                orderBooks[order.instrument].side2.push(topOrder);
                topOrder.quantity = order.quantity;

                order.price = topOrder.price;
                order.status = "Fill";
                order.priorityNumber = -1; // this order doesn't enter orderbooks

                writeExecution(order);
                writeExecution(topOrder);
                cout << "Fill Pfill : " << topOrder.clientOrderID << "\n\n";
            }
            else if (orderBooks[order.instrument].side2.top().quantity < order.quantity)
            {
                OrderData topOrder = orderBooks[order.instrument].side2.top();
                orderBooks[order.instrument].side2.pop();
                topOrder.status = "Fill";

                double temp_order_price = order.price;
                int temp_order_quantity = order.quantity - topOrder.quantity;
                order.price = topOrder.price;
                order.quantity = topOrder.quantity;
                order.status = "Pfill";

                writeExecution(order);
                writeExecution(topOrder);

                cout << "Pfill Fill : " << topOrder.clientOrderID << "\n\n";

                order.price = temp_order_price;
                order.quantity = temp_order_quantity;

                if (orderBooks[order.instrument].side2.empty() || orderBooks[order.instrument].side2.top().price > order.price)
                {
                    order.priorityNumber = ++orderBooks[order.instrument].orders_count_for_price_side1[order.price];
                    orderBooks[order.instrument].side1.push(order);
                }
                else
                {
                    processOrder(order);
                }
            }
        }
    }
    else if (order.side == 2)
    {
        cout << "sell order : " << order.clientOrderID << " " << order.price << endl;
        // if side 1 pq empty no need to execute add new to side 2
        if (orderBooks[order.instrument].side1.empty())
        {
            order.status = "New";
            order.priorityNumber = ++orderBooks[order.instrument].orders_count_for_price_side2[order.price];
            orderBooks[order.instrument].side2.push(order);
            cout << "Side 1 empty. " << order.status << "\n\n";
            writeExecution(order);
        }
        // side1 has a top and not empty
        // top buy price lower than sell order can't execute add sell order
        else if (orderBooks[order.instrument].side1.top().price < order.price)
        {
            order.status = "New";
            order.priorityNumber = ++orderBooks[order.instrument].orders_count_for_price_side2[order.price];
            orderBooks[order.instrument].side2.push(order);
            cout << "Side 1 buy price lower : top buy price : " << orderBooks[order.instrument].side1.top().price << " " << order.status << "\n\n";
            writeExecution(order);
        }
        // execution can happen
        else
        {
            if (orderBooks[order.instrument].side1.top().quantity == order.quantity)
            {
                OrderData topOrder = orderBooks[order.instrument].side1.top();
                topOrder.status = "Fill";
                orderBooks[order.instrument].side1.pop();

                order.price = topOrder.price;
                order.status = "Fill";
                order.priorityNumber = -1; // this order doesn't enter orderbooks

                writeExecution(order);
                writeExecution(topOrder);
                cout << "Fill Fill : " << topOrder.clientOrderID << "\n\n";
            }
            else if (orderBooks[order.instrument].side1.top().quantity > order.quantity)
            {
                OrderData topOrder = orderBooks[order.instrument].side1.top();
                topOrder.quantity -= order.quantity;
                topOrder.status = "Pfill";
                orderBooks[order.instrument].side1.pop();
                orderBooks[order.instrument].side1.push(topOrder);
                topOrder.quantity = order.quantity;

                order.price = topOrder.price;
                order.status = "Fill";
                order.priorityNumber = -1; // this order doesn't enter orderbooks

                writeExecution(order);
                writeExecution(topOrder);
                cout << "Fill Pfill : " << topOrder.clientOrderID << "\n\n";
            }
            else if (orderBooks[order.instrument].side1.top().quantity < order.quantity)
            {
                OrderData topOrder = orderBooks[order.instrument].side1.top();
                orderBooks[order.instrument].side1.pop();
                topOrder.status = "Fill";

                double temp_order_price = order.price;
                int temp_order_quantity = order.quantity - topOrder.quantity;
                order.price = topOrder.price;
                order.quantity = topOrder.quantity;
                order.status = "Pfill";

                writeExecution(order);
                writeExecution(topOrder);

                cout << "Pfill Fill : " << topOrder.clientOrderID << "\n\n";

                order.price = temp_order_price;
                order.quantity = temp_order_quantity;

                if (orderBooks[order.instrument].side1.empty() || orderBooks[order.instrument].side1.top().price < order.price)
                {
                    order.priorityNumber = ++orderBooks[order.instrument].orders_count_for_price_side2[order.price];
                    orderBooks[order.instrument].side2.push(order);
                }
                else
                {
                    processOrder(order);
                }
            }
        }
    }
}

///////////////////////////////////////////// Main Function //////////////////////////////////////////////////////////////

// main function
int main()
{
    if (!reportFile.is_open())
    {
        cerr << "Unable to create execution _rep.csv" << endl;
        return 1;
    }

    // Opening the Orders file
    ifstream OrdersFile("Orders.csv");
    if (!OrdersFile.is_open())
    {
        cerr << "Unable to open Orders.csv" << endl;
        return 1;
    }

    int order_count = 1;
    initializeOrderBooks();

    // Read column titles from the first line
    string line;
    if (getline(OrdersFile, line))
    {
        stringstream ss(line);
        string cell;
        vector<string> columnTitles;

        while (getline(ss, cell, ','))
        {
            columnTitles.push_back(cell);
        }

        // Add colums for execution report
        columnTitles.push_back("Order ID");
        columnTitles.push_back("Exec Status");
        columnTitles.push_back("Reason");
        columnTitles.push_back("Transaction Time");
        columnTitles.push_back("Pr. Seq");

        // Writing column titles including the new ones (Status and Reason)
        reportFile << columnTitles[5] + "," + columnTitles[0] + "," + columnTitles[1] + "," + columnTitles[2] + "," + columnTitles[6] + "," + columnTitles[3] + "," + columnTitles[4] + "," + columnTitles[7] + "," + columnTitles[8] + "," + columnTitles[9] + "\n";

        // Read and process the data
        while (getline(OrdersFile, line))
        {
            stringstream ss(line);
            OrderData order;
            string cell;

            // Parsing the CSV data into OrderData structure
            order.OrderID = "ord" + std::to_string(order_count);
            order_count += 1;

            getline(ss, cell, ',');
            order.clientOrderID = cell;

            getline(ss, cell, ',');
            order.instrument = cell;

            getline(ss, cell, ',');
            order.side = stoi(cell);

            getline(ss, cell, ',');
            order.quantity = stoi(cell);

            getline(ss, cell, ',');
            order.price = stod(cell);

            // start time for rejected transactions
            start_time = std::chrono::high_resolution_clock::now();

            if (validOrder(order))
            {
                // Order accepted
                // Process the orders and generate the Execution Report
                processOrder(order);
            }
            // Order rejected
            else
            {
                order.priorityNumber = -1;
                order.status = "Rejected";
                writeExecution(order);
            }
        }
        OrdersFile.close();
    }
    else
    {
        cerr << "Empty file or missing column titles." << endl;
        OrdersFile.close();
        return 1;
    }

    reportFile.close();
    cout << "Execution Report 'execution _rep.csv' created successfully." << endl;

    for (const auto &flower : FLOWER_TYPES)
    {
        std::string filename_side1 = flower + "_side1.csv";
        std::string filename_side2 = flower + "_side2.csv";

        savePriorityQueueToFile(orderBooks[flower].side1, filename_side1);
        savePriorityQueueToFile(orderBooks[flower].side2, filename_side2);
    }
    cout << "Side 1 and Side 2 priority queue data saved to files successfully." << endl;

    return 0;
}
