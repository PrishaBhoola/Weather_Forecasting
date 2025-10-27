#include <iostream>
#include <string>
#include <vector>
#include <list>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <map>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <unordered_map>

using namespace std;

class Graph_Node {
private:
    string Node_Name;
    vector<int> Children;
    vector<string> Parents;
    int nvalues;
    vector<string> values;
    vector<float> CPT;

public:
    Graph_Node(string name, int n, vector<string> vals) {
        Node_Name = name;
        nvalues = n;
        values = vals;
    }

    string get_name() {
        return Node_Name;
    }

    vector<int> get_children() {
        return Children;
    }

    vector<string> get_Parents() {
        return Parents;
    }

    vector<float> get_CPT() {
        return CPT;
    }

    int get_nvalues() {
        return nvalues;
    }

    vector<string> get_values() {
        return values;
    }

    void set_CPT(vector<float> new_CPT) {
        CPT.clear();
        CPT = new_CPT;
    }

    void set_Parents(vector<string> Parent_Nodes) {
        Parents.clear();
        Parents = Parent_Nodes;
    }

    int add_child(int new_child_index) {
        for(int i = 0; i < Children.size(); i++) {
            if(Children[i] == new_child_index)
                return 0;
        }
        Children.push_back(new_child_index);
        return 1;
    }
};

class network {
    list<Graph_Node> Pres_Graph;

public:
    int addNode(Graph_Node node) {
        Pres_Graph.push_back(node);
        return 0;
    }

    list<Graph_Node>::iterator getNode(int i) {
        int count = 0;
        list<Graph_Node>::iterator listIt;
        for(listIt = Pres_Graph.begin(); listIt != Pres_Graph.end(); listIt++) {
            if(count++ == i)
                break;
        }
        return listIt;
    }

    int netSize() {
        return Pres_Graph.size();
    }

    int get_index(string val_name) {
        list<Graph_Node>::iterator listIt;
        int count = 0;
        for(listIt = Pres_Graph.begin(); listIt != Pres_Graph.end(); listIt++) {
            if(listIt->get_name().compare(val_name) == 0)
                return count;
            count++;
        }
        return -1;
    }

    list<Graph_Node>::iterator get_nth_node(int n) {
        list<Graph_Node>::iterator listIt;
        int count = 0;
        for(listIt = Pres_Graph.begin(); listIt != Pres_Graph.end(); listIt++) {
            if(count == n)
                return listIt;
            count++;
        }
        return listIt;
    }

    list<Graph_Node>::iterator search_node(string val_name) {
        list<Graph_Node>::iterator listIt;
        for(listIt = Pres_Graph.begin(); listIt != Pres_Graph.end(); listIt++) {
            if(listIt->get_name().compare(val_name) == 0)
                return listIt;
        }
        cout << "node not found: " << val_name << "\n";
        return listIt;
    }
};

// Helper function to trim whitespace
string trim(const string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (string::npos == first) {
        return str;
    }
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

network read_network(const char* filename) {
    network BayesNet;
    string line;
    ifstream myfile(filename);

    if (!myfile.is_open()) {
        cout << "Error: Could not open file " << filename << endl;
        return BayesNet;
    }

    while (getline(myfile, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }

        stringstream ss(line);
        string token;
        ss >> token;

        if (token == "variable") {
            string var_name;
            ss >> var_name;

            getline(myfile, line);
            stringstream ss2(line);
            string type_keyword, discrete_keyword, bracket, equals;
            int num_values;
            ss2 >> type_keyword >> discrete_keyword >> bracket >> num_values >> bracket >> equals >> bracket;

            vector<string> values;
            string value;
            while (ss2 >> value) {
                if (value == "};") break;
                if (value.back() == ',') {
                    value = value.substr(0, value.length() - 1);
                }
                values.push_back(value);
            }

            Graph_Node new_node(var_name, num_values, values);
            BayesNet.addNode(new_node);
        }
        else if (token == "probability") {
            string paren, node_name;
            ss >> paren >> node_name;

            string full_prob_line = line;
            while (full_prob_line.find('{') == string::npos) {
                string next_line;
                if (!getline(myfile, next_line)) break;
                full_prob_line += " " + trim(next_line);
            }

            size_t start_paren = full_prob_line.find('(');
            size_t end_paren = full_prob_line.find(')');
            size_t pipe_pos = full_prob_line.find('|');

            if (start_paren == string::npos || end_paren == string::npos) {
                continue;
            }

            string prob_content = full_prob_line.substr(start_paren + 1, end_paren - start_paren - 1);
            stringstream prob_ss(prob_content);
            prob_ss >> node_name;

            list<Graph_Node>::iterator listIt = BayesNet.search_node(node_name);
            int index = BayesNet.get_index(node_name);
            vector<string> parents;

            if (pipe_pos != string::npos && pipe_pos < end_paren) {
                string parents_str = full_prob_line.substr(pipe_pos + 1, end_paren - pipe_pos - 1);
                stringstream parent_ss(parents_str);
                string parent;
                while (parent_ss >> parent) {
                    if (parent.back() == ',') {
                        parent = parent.substr(0, parent.length() - 1);
                    }
                    parents.push_back(parent);
                    list<Graph_Node>::iterator parentIt = BayesNet.search_node(parent);
                    parentIt->add_child(index);
                }
                listIt->set_Parents(parents);
            }

            vector<float> cpt;
            while (getline(myfile, line)) {
                line = trim(line);
                if (line == "};") break;
                if (line.empty()) continue;

                size_t close_paren = line.find(')');
                string prob_part;

                if (close_paren != string::npos) {
                    prob_part = line.substr(close_paren + 1);
                } else if (line.find("table") != string::npos) {
                    size_t table_pos = line.find("table");
                    prob_part = line.substr(table_pos + 5);
                } else {
                    prob_part = line;
                }

                stringstream ss_prob(prob_part);
                string token;
                while (ss_prob >> token) {
                    while (!token.empty() && (token.back() == ',' || token.back() == ';')) {
                        token = token.substr(0, token.length() - 1);
                    }
                    if (!token.empty() && (isdigit(token[0]) || token[0] == '.' || token[0] == '-')) {
                        cpt.push_back(atof(token.c_str()));
                    }
                }
            }
            listIt->set_CPT(cpt);
        }
    }

    myfile.close();
    return BayesNet;
}

void write_network(const char* filename, network& BayesNet) {
    ofstream outfile(filename);

    if (!outfile.is_open()) {
        cout << "Error: Could not open file " << filename << " for writing" << endl;
        return;
    }

    outfile << "// Bayesian Network" << endl << endl;

    int N = BayesNet.netSize();

    for (int i = 0; i < N; i++) {
        auto node = BayesNet.get_nth_node(i);
        outfile << "variable " << node->get_name() << " {" << endl;
        outfile << "  type discrete [ " << node->get_nvalues() << " ] = { ";
        vector<string> vals = node->get_values();
        for (int j = 0; j < (int)vals.size(); j++) {
            outfile << vals[j];
            if (j < (int)vals.size() - 1) outfile << ", ";
        }
        outfile << " };" << endl;
        outfile << "}" << endl;
    }

    outfile << fixed << setprecision(4);

    for (int i = 0; i < N; i++) {
        auto node = BayesNet.get_nth_node(i);
        vector<string> parents = node->get_Parents();
        vector<string> values = node->get_values();
        vector<float> cpt = node->get_CPT();

        outfile << "probability ( " << node->get_name();
        if (!parents.empty()) {
            outfile << " | ";
            for (int j = 0; j < (int)parents.size(); j++) {
                outfile << parents[j];
                if (j < (int)parents.size() - 1) outfile << ", ";
            }
        }
        outfile << " ) {" << endl;

        vector<int> radices;
        radices.reserve(parents.size());
        for (auto &pname : parents) {
            auto pnode = BayesNet.search_node(pname);
            radices.push_back(pnode->get_nvalues());
        }

        int parent_combinations = 1;
        for (int r : radices) parent_combinations *= r;

        int cpt_index = 0;
        if (parents.empty()) {
            outfile << "  table ";
            for (int k = 0; k < (int)values.size(); k++) {
                if (cpt_index < (int)cpt.size()) outfile << cpt[cpt_index++];
                else outfile << "-1";
                if (k < (int)values.size() - 1) outfile << ", ";
            }
            outfile << ";" << endl;
        } else {
            for (int comb = 0; comb < parent_combinations; comb++) {
                vector<int> idx(parents.size(), 0);
                int tmp = comb;
                for (int p = (int)parents.size() - 1; p >= 0; p--) {
                    idx[p] = tmp % radices[p];
                    tmp /= radices[p];
                }

                outfile << "  ( ";
                for (int p = 0; p < (int)parents.size(); p++) {
                    auto pnode = BayesNet.search_node(parents[p]);
                    auto pvals = pnode->get_values();
                    int vidx = idx[p];
                    outfile << pvals[vidx];
                    if (p < (int)parents.size() - 1) outfile << ", ";
                }
                outfile << " ) ";

                for (int k = 0; k < (int)values.size(); k++) {
                    if (cpt_index < (int)cpt.size()) outfile << cpt[cpt_index++];
                    else outfile << "-1";
                    if (k < (int)values.size() - 1) outfile << ", ";
                }
                outfile << ";" << endl;
            }
        }
        outfile << "};" << endl << endl;
    }

    outfile.close();
    cout << "Network written to file: " << filename << endl;
}

#ifndef BN_LIB

struct DataRecord {
    vector<string> values;
    int missing_index;
    DataRecord(int size) : values(size), missing_index(-1) {}
};

vector<DataRecord> read_records(const char* filename, network& BayesNet) {
    vector<DataRecord> records;
    ifstream file(filename);

    if (!file.is_open()) {
        cout << "Error: Could not open " << filename << endl;
        return records;
    }

    int num_vars = BayesNet.netSize();
    string line;

    while (getline(file, line)) {
        DataRecord record(num_vars);
        stringstream ss(line);
        string value;
        int var_idx = 0;

        while (getline(ss, value, ',')) {
            value.erase(remove(value.begin(), value.end(), '"'), value.end());
            value.erase(remove(value.begin(), value.end(), ' '), value.end());

            if (value == "?") {
                record.missing_index = var_idx;
            }
            record.values[var_idx] = value;
            var_idx++;
        }

        if (var_idx == num_vars) {
            records.push_back(record);
        }
    }

    file.close();
    cout << "Loaded " << records.size() << " records" << endl;
    return records;
}

int get_value_index(const vector<string>& values, const string& value) {
    for (int i = 0; i < values.size(); i++) {
        if (values[i] == value) return i;
    }
    return -1;
}

// Helper function: check if missing variable is a parent
bool is_parent_missing(const vector<int>& parent_indices, int missing_var) {
    for (int pidx : parent_indices) {
        if (pidx == missing_var) return true;
    }
    return false;
}

// Helper function: compute parent combination index
int get_parent_combo(network& BayesNet, const vector<int>& parent_indices, 
                     const vector<int>& parent_sizes, const DataRecord& record) {
    if (parent_indices.empty()) return 0;

    int parent_combo = 0;
    int multiplier = 1;

    for (int p = parent_indices.size() - 1; p >= 0; p--) {
        int pidx = parent_indices[p];
        auto pnode = BayesNet.get_nth_node(pidx);
        int p_val_idx = get_value_index(pnode->get_values(), record.values[pidx]);

        if (p_val_idx == -1) return -1;

        parent_combo += p_val_idx * multiplier;
        multiplier *= parent_sizes[p];
    }

    return parent_combo;
}

// Helper function: compute parent combo when one parent has a hypothetical value
int get_parent_combo_with_value(network& BayesNet, const vector<int>& parent_indices, const vector<int>& parent_sizes, const DataRecord& record, int special_pos, const string& special_value) {
    if (parent_indices.empty()) return 0;

    int parent_combo = 0;
    int multiplier = 1;

    for (int p = parent_indices.size() - 1; p >= 0; p--) {
        int pidx = parent_indices[p];
        auto pnode = BayesNet.get_nth_node(pidx);

        int p_val_idx;
        if (p == special_pos) {
            p_val_idx = get_value_index(pnode->get_values(), special_value);
        } else {
            p_val_idx = get_value_index(pnode->get_values(), record.values[pidx]);
        }

        if (p_val_idx == -1) return -1;

        parent_combo += p_val_idx * multiplier;
        multiplier *= parent_sizes[p];
    }

    return parent_combo;
}

// D-MAR Initialization: Uses incomplete records intelligently
void initialize_cpts_dmar(network& BayesNet, const vector<DataRecord>& records) {
    cout << "Using D-MAR initialization for better parameter estimates..." << endl;

    int num_nodes = BayesNet.netSize();

    // First pass: compute marginal distributions for all variables
    // (used for weighting when a parent is missing)
    vector<map<string, double>> marginal_counts(num_nodes);
    vector<double> marginal_totals(num_nodes, 0.0);

    for (const DataRecord& record : records) {
        for (int var_idx = 0; var_idx < num_nodes; var_idx++) {
            if (record.missing_index != var_idx && record.values[var_idx] != "?") {
                marginal_counts[var_idx][record.values[var_idx]] += 1.0;
                marginal_totals[var_idx] += 1.0;
            }
        }
    }

    // Second pass: estimate parameters for each node
    for (int node_idx = 0; node_idx < num_nodes; node_idx++) {
        auto node = BayesNet.get_nth_node(node_idx);
        vector<string> parents = node->get_Parents();
        vector<string> node_values = node->get_values();
        int num_values = node_values.size();

        // Get parent information
        vector<int> parent_indices;
        vector<int> parent_sizes;
        for (const string& parent_name : parents) {
            int p_idx = BayesNet.get_index(parent_name);
            parent_indices.push_back(p_idx);
            auto p_node = BayesNet.get_nth_node(p_idx);
            parent_sizes.push_back(p_node->get_nvalues());
        }

        // Calculate number of parent combinations
        int num_parent_combos = 1;
        for (int size : parent_sizes) {
            num_parent_combos *= size;
        }

        // Initialize counts with small Laplace smoothing
        vector<vector<double>> counts(num_parent_combos, vector<double>(num_values, 1.0));

        // D-MAR: Process records intelligently
        for (const DataRecord& record : records) {
            int missing_var = record.missing_index;

            // Case 1: Complete record - use full weight
            if (missing_var == -1) {
                int parent_combo = get_parent_combo(BayesNet, parent_indices, parent_sizes, record);
                if (parent_combo >= 0) {
                    int val_idx = get_value_index(node_values, record.values[node_idx]);
                    if (val_idx != -1) {
                        counts[parent_combo][val_idx] += 1.0;
                    }
                }
            }
            // Case 2: This node is missing - skip
            else if (missing_var == node_idx) {
                continue;
            }
            // Case 3: One of the parents is missing - use weighted contribution
            else if (is_parent_missing(parent_indices, missing_var)) {
                int val_idx = get_value_index(node_values, record.values[node_idx]);
                if (val_idx == -1) continue;

                // Find which parent is missing
                int missing_parent_pos = -1;
                for (int p = 0; p < parent_indices.size(); p++) {
                    if (parent_indices[p] == missing_var) {
                        missing_parent_pos = p;
                        break;
                    }
                }

                // Get possible values for missing parent
                auto missing_parent_node = BayesNet.get_nth_node(missing_var);
                vector<string> missing_parent_values = missing_parent_node->get_values();

                // Weight by marginal probability of each parent value
                for (const string& pval : missing_parent_values) {
                    double weight = 0.0;
                    if (marginal_totals[missing_var] > 0) {
                        weight = marginal_counts[missing_var][pval] / marginal_totals[missing_var];
                    } else {
                        weight = 1.0 / missing_parent_values.size();
                    }

                    int parent_combo = get_parent_combo_with_value(
                        BayesNet, parent_indices, parent_sizes, record, 
                        missing_parent_pos, pval
                    );

                    if (parent_combo >= 0) {
                        counts[parent_combo][val_idx] += weight;
                    }
                }
            }
            // Case 4: Unrelated variable is missing - use full weight
            else {
                int parent_combo = get_parent_combo(BayesNet, parent_indices, parent_sizes, record);
                if (parent_combo >= 0) {
                    int val_idx = get_value_index(node_values, record.values[node_idx]);
                    if (val_idx != -1) {
                        counts[parent_combo][val_idx] += 1.0;
                    }
                }
            }
        }

        // Convert counts to probabilities
        vector<float> cpt;
        for (int combo = 0; combo < num_parent_combos; combo++) {
            double sum = 0.0;
            for (int v = 0; v < num_values; v++) {
                sum += counts[combo][v];
            }
            for (int v = 0; v < num_values; v++) {
                cpt.push_back(counts[combo][v] / sum);
            }
        }
        node->set_CPT(cpt);
    }

    cout << "D-MAR initialization complete." << endl;
}

double get_probability(network& BayesNet, int node_idx, int value_idx,
                      const vector<int>& parent_indices, const vector<int>& parent_value_indices) {
    auto node = BayesNet.get_nth_node(node_idx);
    vector<float> cpt = node->get_CPT();

    if (parent_indices.empty()) {
        return cpt[value_idx];
    }

    vector<int> parent_sizes;
    for (int p_idx : parent_indices) {
        parent_sizes.push_back(BayesNet.get_nth_node(p_idx)->get_nvalues());
    }

    int parent_combo = 0;
    int multiplier = 1;
    for (int p = parent_indices.size() - 1; p >= 0; p--) {
        parent_combo += parent_value_indices[p] * multiplier;
        multiplier *= parent_sizes[p];
    }

    int num_values = node->get_nvalues();
    int cpt_idx = parent_combo * num_values + value_idx;
    return (cpt_idx < cpt.size()) ? cpt[cpt_idx] : 0.0;
}

void em_iteration(network& BayesNet, const vector<DataRecord>& records) {
    int num_nodes = BayesNet.netSize();

    // Storage for expected counts
    vector<map<int, vector<double>>> expected_counts(num_nodes);

    // Initialize expected counts with Laplace smoothing
    for (int node_idx = 0; node_idx < num_nodes; node_idx++) {
        auto node = BayesNet.get_nth_node(node_idx);
        int num_values = node->get_nvalues();
        vector<string> parents = node->get_Parents();

        int num_parent_combos = 1;
        if (!parents.empty()) {
            for (const string& parent_name : parents) {
                int p_idx = BayesNet.get_index(parent_name);
                num_parent_combos *= BayesNet.get_nth_node(p_idx)->get_nvalues();
            }
        }

        for (int combo = 0; combo < num_parent_combos; combo++) {
            expected_counts[node_idx][combo] = vector<double>(num_values, 0.37);
        }
    }

    // Process each record
    for (const DataRecord& record : records) {
        if (record.missing_index == -1) {
            // Complete record - add full count
            for (int node_idx = 0; node_idx < num_nodes; node_idx++) {
                auto node = BayesNet.get_nth_node(node_idx);
                vector<string> parents = node->get_Parents();
                vector<string> node_values = node->get_values();

                // Get parent indices
                vector<int> parent_indices;
                vector<int> parent_sizes;
                for (const string& parent_name : parents) {
                    int p_idx = BayesNet.get_index(parent_name);
                    parent_indices.push_back(p_idx);
                    parent_sizes.push_back(BayesNet.get_nth_node(p_idx)->get_nvalues());
                }

                // Get parent combination
                int parent_combo = 0;
                if (!parents.empty()) {
                    int multiplier = 1;
                    for (int p = parent_indices.size() - 1; p >= 0; p--) {
                        int p_val_idx = get_value_index(
                            BayesNet.get_nth_node(parent_indices[p])->get_values(),
                            record.values[parent_indices[p]]
                        );
                        parent_combo += p_val_idx * multiplier;
                        multiplier *= parent_sizes[p];
                    }
                }

                int val_idx = get_value_index(node_values, record.values[node_idx]);
                if (val_idx != -1) {
                    expected_counts[node_idx][parent_combo][val_idx] += 1.0;
                }
            }
        } else {
            // Incomplete record - E-step
            int missing_var = record.missing_index;
            auto missing_node = BayesNet.get_nth_node(missing_var);
            vector<string> missing_values = missing_node->get_values();
            vector<string> missing_parents = missing_node->get_Parents();

            // Get parent values for missing variable
            vector<int> missing_parent_indices;
            vector<int> missing_parent_val_indices;
            for (const string& parent_name : missing_parents) {
                int p_idx = BayesNet.get_index(parent_name);
                missing_parent_indices.push_back(p_idx);
                int p_val_idx = get_value_index(
                    BayesNet.get_nth_node(p_idx)->get_values(),
                    record.values[p_idx]
                );
                missing_parent_val_indices.push_back(p_val_idx);
            }

            // Calculate P(missing_var = each_value | observed) using CPT
            vector<double> probs(missing_values.size());
            double sum_probs = 0.0;
            for (int v = 0; v < missing_values.size(); v++) {
                double prob = get_probability(BayesNet, missing_var, v,
                                             missing_parent_indices, missing_parent_val_indices);
                probs[v] = prob;
                sum_probs += prob;
            }

            // Normalize probabilities
            if (sum_probs > 0) {
                for (int v = 0; v < missing_values.size(); v++) {
                    probs[v] /= sum_probs;
                }
            } else {
                for (int v = 0; v < missing_values.size(); v++) {
                    probs[v] = 1.0 / missing_values.size();
                }
            }

            // Add fractional counts for all variables
            for (int node_idx = 0; node_idx < num_nodes; node_idx++) {
                auto node = BayesNet.get_nth_node(node_idx);
                vector<string> parents = node->get_Parents();
                vector<string> node_values = node->get_values();

                vector<int> parent_indices;
                vector<int> parent_sizes;
                for (const string& parent_name : parents) {
                    int p_idx = BayesNet.get_index(parent_name);
                    parent_indices.push_back(p_idx);
                    parent_sizes.push_back(BayesNet.get_nth_node(p_idx)->get_nvalues());
                }

                if (node_idx == missing_var) {
                    // This is the missing variable
                    int parent_combo = 0;
                    if (!parents.empty()) {
                        int multiplier = 1;
                        for (int p = parent_indices.size() - 1; p >= 0; p--) {
                            int p_val_idx = get_value_index(
                                BayesNet.get_nth_node(parent_indices[p])->get_values(),
                                record.values[parent_indices[p]]
                            );
                            parent_combo += p_val_idx * multiplier;
                            multiplier *= parent_sizes[p];
                        }
                    }

                    for (int v = 0; v < missing_values.size(); v++) {
                        expected_counts[node_idx][parent_combo][v] += probs[v];
                    }
                } else {
                    // Check if missing variable is a parent of this node
                    bool missing_is_parent = false;
                    int missing_parent_pos = -1;
                    for (int p = 0; p < parent_indices.size(); p++) {
                        if (parent_indices[p] == missing_var) {
                            missing_is_parent = true;
                            missing_parent_pos = p;
                            break;
                        }
                    }

                    int val_idx = get_value_index(node_values, record.values[node_idx]);

                    if (missing_is_parent) {
                        // Need to sum over possible values of missing parent
                        for (int mv = 0; mv < missing_values.size(); mv++) {
                            int parent_combo = 0;
                            int multiplier = 1;
                            for (int p = parent_indices.size() - 1; p >= 0; p--) {
                                int p_val_idx;
                                if (p == missing_parent_pos) {
                                    p_val_idx = mv;
                                } else {
                                    p_val_idx = get_value_index(
                                        BayesNet.get_nth_node(parent_indices[p])->get_values(),
                                        record.values[parent_indices[p]]
                                    );
                                }
                                parent_combo += p_val_idx * multiplier;
                                multiplier *= parent_sizes[p];
                            }

                            if (val_idx != -1) {
                                expected_counts[node_idx][parent_combo][val_idx] += probs[mv];
                            }
                        }
                    } else {
                        // Missing variable is not involved
                        int parent_combo = 0;
                        if (!parents.empty()) {
                            int multiplier = 1;
                            for (int p = parent_indices.size() - 1; p >= 0; p--) {
                                int p_val_idx = get_value_index(
                                    BayesNet.get_nth_node(parent_indices[p])->get_values(),
                                    record.values[parent_indices[p]]
                                );
                                parent_combo += p_val_idx * multiplier;
                                multiplier *= parent_sizes[p];
                            }
                        }

                        if (val_idx != -1) {
                            expected_counts[node_idx][parent_combo][val_idx] += 1.0;
                        }
                    }
                }
            }
        }
    }

    // M-step: Update CPTs
    for (int node_idx = 0; node_idx < num_nodes; node_idx++) {
        auto node = BayesNet.get_nth_node(node_idx);
        int num_values = node->get_nvalues();
        vector<float> new_cpt;

        for (auto& entry : expected_counts[node_idx]) {
            double sum = 0.0;
            for (double count : entry.second) {
                sum += count;
            }
            for (double count : entry.second) {
                new_cpt.push_back(count / sum);
            }
        }
        node->set_CPT(new_cpt);
    }
}

// Add this helper to measure CPT changes between iterations
double compute_cpt_distance(network& BayesNet, const vector<vector<float>>& old_cpts) {
    double total_diff = 0.0;
    int num_nodes = BayesNet.netSize();
    
    for (int i = 0; i < num_nodes; i++) {
        auto node = BayesNet.get_nth_node(i);
        vector<float> current_cpt = node->get_CPT();
        
        if (i < old_cpts.size() && old_cpts[i].size() == current_cpt.size()) {
            for (size_t j = 0; j < current_cpt.size(); j++) {
                total_diff += fabs(current_cpt[j] - old_cpts[i][j]);
            }
        }
    }
    
    return total_diff;
}

// Save all CPTs
vector<vector<float>> save_all_cpts(network& BayesNet) {
    vector<vector<float>> all_cpts;
    int num_nodes = BayesNet.netSize();
    
    for (int i = 0; i < num_nodes; i++) {
        auto node = BayesNet.get_nth_node(i);
        all_cpts.push_back(node->get_CPT());
    }
    
    return all_cpts;
}

int main() {
    auto start_time = chrono::high_resolution_clock::now();

    // Read network structure
    network BayesNet = read_network("hailfinder.bif");
    cout << "Network loaded: " << BayesNet.netSize() << " nodes" << endl;

    // Read records
    vector<DataRecord> records = read_records("records.dat", BayesNet);

    // Count complete vs incomplete records
    int complete = 0, incomplete = 0;
    for (const auto& rec : records) {
        if (rec.missing_index == -1) complete++;
        else incomplete++;
    }
    cout << "Complete records: " << complete << ", Incomplete: " << incomplete << endl;

    // Initialize CPTs with D-MAR algorithm
    cout << "Initializing CPTs with D-MAR algorithm..." << endl;
    initialize_cpts_dmar(BayesNet, records);

    // Save initial CPTs for convergence checking
    vector<vector<float>> prevcpts = save_all_cpts(BayesNet);

    // Run EM iterations
    cout << "Running EM algorithm to refine parameters..." << endl;
    int iteration = 0;
    const double max_time_seconds = 114.0;  // 1.9 minutes

    // Track convergence
    vector<double> changes;
    int convergedcount = 0;
    const double convergencethreshold = 1e-9;

    while (true) {
        auto current_time = chrono::high_resolution_clock::now();
        chrono::duration<double> elapsed = current_time - start_time;

        if (elapsed.count() > max_time_seconds) {
            cout << "Time limit reached after " << iteration << " iterations" << endl;
            break;
        }

        em_iteration(BayesNet, records);
        iteration++;

        if (iteration % 10 == 0) {
            vector<vector<float>> currentcpts = save_all_cpts(BayesNet);
            double change = compute_cpt_distance(BayesNet, prevcpts);
            changes.push_back(change);
            cout << "Completed " << iteration << " iterations (time: "
                 << elapsed.count() << "s)" << endl;

            // Check if converged
            if (change < convergencethreshold) {
                convergedcount++;
                if (convergedcount >= 3) {
                    cout << "\n*** CONVERGED: CPT changes < " << convergencethreshold 
                            << " for 3 consecutive checks ***" << endl;
                    cout << "Stopping at iteration " << iteration << endl;
                    break;
                }
            } else {
                convergedcount = 0;
            }

            prevcpts = currentcpts;
        }
    }

    // Write output
    cout << "Writing output to solved.bif..." << endl;
    write_network("solved.bif", BayesNet);

    auto end_time = chrono::high_resolution_clock::now();
    chrono::duration<double> total_time = end_time - start_time;
    cout << "Total time: " << total_time.count() << " seconds" << endl;
    cout << "Done!" << endl;

    return 0;
}

#endif // BN_LIB
