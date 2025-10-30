#define BN_LIB
#include "starter.cpp"
#include <algorithm>
#include <chrono>

using namespace std;

// Structure to store a data record
struct Record {
    vector<string> values;  // values[i] = value of i-th variable, or "?" if missing
    int missing_index;      // index of missing variable, or -1 if complete
    
    Record() : missing_index(-1) {}
};

// Helper function to remove quotes from values
string remove_quotes(string s) {
    if (s.length() >= 2 && s[0] == '"' && s[s.length()-1] == '"') {
        return s.substr(1, s.length()-2);
    }
    return s;
}

// Read all records from data file
vector<Record> read_records(const char* filename, int num_vars) {
    vector<Record> records;
    ifstream infile(filename);
    
    if (!infile.is_open()) {
        cout << "Error: Could not open data file " << filename << endl;
        return records;
    }
    
    string line;
    while (getline(infile, line)) {
        if (line.empty()) continue;
        
        Record rec;
        rec.values.resize(num_vars);
        rec.missing_index = -1;
        
        // Parse CSV
        stringstream ss(line);
        string value;
        int idx = 0;
        
        while (getline(ss, value, ',') && idx < num_vars) {
            value = trim(value);
            value = remove_quotes(value);
            
            if (value == "?") {
                rec.missing_index = idx;
            }
            rec.values[idx] = value;
            idx++;
        }
        
        records.push_back(rec);
    }
    
    infile.close();
    return records;
}

// Get index of a value in a node's value list
int get_value_index(const vector<string>& values, const string& value) {
    for (int i = 0; i < values.size(); i++) {
        if (values[i] == value) return i;
    }
    return -1;
}

// Compute CPT index from parent values
int compute_cpt_index(network& net, const vector<string>& parents, 
                      const vector<string>& parent_values, int child_value_idx) {
    if (parents.empty()) {
        return child_value_idx;
    }
    
    // Compute parent configuration index
    int parent_config = 0;
    int multiplier = 1;
    
    // Iterate parents from right to left (rightmost cycles fastest)
    for (int p = parents.size() - 1; p >= 0; p--) {
        auto parent_node = net.search_node(parents[p]);
        auto parent_vals = parent_node->get_values();
        
        int parent_val_idx = get_value_index(parent_vals, parent_values[p]);
        if (parent_val_idx < 0) return -1;
        
        parent_config += parent_val_idx * multiplier;
        multiplier *= parent_vals.size();
    }
    
    // Total index = parent_config * num_child_values + child_value_idx
    auto node = net.search_node(parents[0]); // dummy, we need child node
    // Actually we need the child node's nvalues
    // Let me fix this - we pass child_nvalues
    
    return -1; // placeholder, will fix
}

// Better version: compute CPT index given parent indices
int compute_cpt_index_v2(const vector<int>& parent_radices, 
                         const vector<int>& parent_indices, 
                         int child_value_idx, int child_nvalues) {
    if (parent_radices.empty()) {
        return child_value_idx;
    }
    
    // Compute parent configuration index
    int parent_config = 0;
    int multiplier = 1;
    
    for (int p = parent_radices.size() - 1; p >= 0; p--) {
        parent_config += parent_indices[p] * multiplier;
        multiplier *= parent_radices[p];
    }
    
    return parent_config * child_nvalues + child_value_idx;
}

// Track which CPT indices need learning (returns mask of indices to learn)
vector<vector<bool>> identify_learning_params(network& net) {
    int N = net.netSize();
    vector<vector<bool>> learn_mask(N);
    
    for (int i = 0; i < N; i++) {
        auto node = net.get_nth_node(i);
        vector<float> cpt = node->get_CPT();
        learn_mask[i].resize(cpt.size(), false);
        
        for (int j = 0; j < cpt.size(); j++) {
            if (cpt[j] < 0) {
                learn_mask[i][j] = true;
            }
        }
    }
    
    return learn_mask;
}

// Initialize CPT from complete records using MLE with Laplace smoothing
void initialize_cpt_from_complete(network& net, const vector<Record>& records, 
                                  const vector<vector<bool>>& learn_mask, double alpha) {
    int N = net.netSize();
    
    // For each node, count occurrences
    for (int i = 0; i < N; i++) {
        auto node = net.get_nth_node(i);
        vector<string> parents = node->get_Parents();
        vector<string> values = node->get_values();
        vector<float> cpt = node->get_CPT();
        
        // Check if any parameters need learning
        bool needs_learning = false;
        for (bool mask_val : learn_mask[i]) {
            if (mask_val) {
                needs_learning = true;
                break;
            }
        }
        
        if (!needs_learning) continue;
        
        // Compute parent radices
        vector<int> parent_radices;
        for (const auto& pname : parents) {
            auto pnode = net.search_node(pname);
            parent_radices.push_back(pnode->get_nvalues());
        }
        
        int parent_combinations = 1;
        for (int r : parent_radices) parent_combinations *= r;
        
        // Initialize counts with Laplace smoothing
        vector<double> counts(cpt.size(), alpha);
        
        // Count from ALL records - use uniform distribution for missing values
        for (const auto& rec : records) {
            // Skip if this node itself is missing
            if (rec.missing_index == i) continue;
            
            // Get parent values
            vector<int> parent_indices;
            bool valid = true;
            bool has_missing_parent = false;
            int missing_parent_pos = -1;
            
            for (int p = 0; p < parents.size(); p++) {
                const auto& pname = parents[p];
                int parent_idx = net.get_index(pname);
                
                if (rec.missing_index == parent_idx) {
                    has_missing_parent = true;
                    missing_parent_pos = p;
                    parent_indices.push_back(0); // placeholder
                } else {
                    auto pnode = net.search_node(pname);
                    auto pvals = pnode->get_values();
                    
                    int val_idx = get_value_index(pvals, rec.values[parent_idx]);
                    if (val_idx < 0) {
                        valid = false;
                        break;
                    }
                    parent_indices.push_back(val_idx);
                }
            }
            
            if (!valid) continue;
            
            // Get child value
            int child_val_idx = get_value_index(values, rec.values[i]);
            if (child_val_idx < 0) continue;
            
            // If a parent is missing, distribute uniformly across its possible values
            if (has_missing_parent) {
                int mp_idx = net.get_index(parents[missing_parent_pos]);
                auto mp_node = net.search_node(parents[missing_parent_pos]);
                int num_mp_values = mp_node->get_nvalues();
                double weight = 1.0 / num_mp_values;
                
                for (int mp_val = 0; mp_val < num_mp_values; mp_val++) {
                    parent_indices[missing_parent_pos] = mp_val;
                    int cpt_idx = compute_cpt_index_v2(parent_radices, parent_indices, 
                                                       child_val_idx, values.size());
                    if (cpt_idx >= 0 && cpt_idx < counts.size()) {
                        counts[cpt_idx] += weight;
                    }
                }
            } else {
                // All parents observed - full count
                int cpt_idx = compute_cpt_index_v2(parent_radices, parent_indices, 
                                                   child_val_idx, values.size());
                if (cpt_idx >= 0 && cpt_idx < counts.size()) {
                    counts[cpt_idx] += 1.0;
                }
            }
        }
        
        // Normalize to get probabilities
        vector<float> new_cpt = cpt;
        
        if (parents.empty()) {
            // Unconditional probability
            double sum = 0;
            for (int k = 0; k < values.size(); k++) {
                sum += counts[k];
            }
            for (int k = 0; k < values.size(); k++) {
                if (learn_mask[i][k]) { // only update if needs learning
                    new_cpt[k] = counts[k] / sum;
                }
            }
        } else {
            // Conditional probability - normalize for each parent configuration
            for (int pc = 0; pc < parent_combinations; pc++) {
                double sum = 0;
                for (int k = 0; k < values.size(); k++) {
                    int idx = pc * values.size() + k;
                    sum += counts[idx];
                }
                for (int k = 0; k < values.size(); k++) {
                    int idx = pc * values.size() + k;
                    if (learn_mask[i][idx]) { // only update if needs learning
                        new_cpt[idx] = counts[idx] / sum;
                    }
                }
            }
        }
        
        node->set_CPT(new_cpt);
    }
}

// EM Algorithm - E-step: compute expected counts from incomplete records
void em_e_step(network& net, const vector<Record>& records, 
               vector<vector<double>>& expected_counts, double alpha) {
    int N = net.netSize();
    
    // Initialize expected counts with alpha (Laplace smoothing)
    for (int i = 0; i < N; i++) {
        auto node = net.get_nth_node(i);
        expected_counts[i].assign(node->get_CPT().size(), alpha);
    }
    
    // Add complete records
    for (const auto& rec : records) {
        if (rec.missing_index >= 0) continue; // skip incomplete for now
        
        for (int i = 0; i < N; i++) {
            auto node = net.get_nth_node(i);
            vector<string> parents = node->get_Parents();
            vector<string> values = node->get_values();
            
            // Compute parent radices
            vector<int> parent_radices;
            for (const auto& pname : parents) {
                auto pnode = net.search_node(pname);
                parent_radices.push_back(pnode->get_nvalues());
            }
            
            // Get parent indices
            vector<int> parent_indices;
            bool valid = true;
            for (const auto& pname : parents) {
                int parent_idx = net.get_index(pname);
                auto pnode = net.search_node(pname);
                auto pvals = pnode->get_values();
                
                int val_idx = get_value_index(pvals, rec.values[parent_idx]);
                if (val_idx < 0) {
                    valid = false;
                    break;
                }
                parent_indices.push_back(val_idx);
            }
            
            if (!valid) continue;
            
            // Get child value
            int child_val_idx = get_value_index(values, rec.values[i]);
            if (child_val_idx < 0) continue;
            
            // Update count
            int cpt_idx = compute_cpt_index_v2(parent_radices, parent_indices, 
                                               child_val_idx, values.size());
            if (cpt_idx >= 0 && cpt_idx < expected_counts[i].size()) {
                expected_counts[i][cpt_idx] += 1.0;
            }
        }
    }
    
    // Add expected counts from incomplete records
    for (const auto& rec : records) {
        if (rec.missing_index < 0) continue; // skip complete records
        
        int miss_idx = rec.missing_index;
        auto miss_node = net.get_nth_node(miss_idx);
        auto miss_values = miss_node->get_values();
        auto miss_parents = miss_node->get_Parents();
        
        // Compute P(missing_var = v | parents, other_evidence) for each possible value
        vector<double> probs(miss_values.size(), 1.0);
        
        // Get parent values for missing variable
        vector<int> miss_parent_radices;
        vector<int> miss_parent_indices;
        bool valid = true;
        
        for (const auto& pname : miss_parents) {
            int parent_idx = net.get_index(pname);
            auto pnode = net.search_node(pname);
            auto pvals = pnode->get_values();
            miss_parent_radices.push_back(pvals.size());
            
            int val_idx = get_value_index(pvals, rec.values[parent_idx]);
            if (val_idx < 0) {
                valid = false;
                break;
            }
            miss_parent_indices.push_back(val_idx);
        }
        
        if (valid) {
            // Get probabilities from CPT of missing variable
            vector<float> miss_cpt = miss_node->get_CPT();
            for (int v = 0; v < miss_values.size(); v++) {
                int cpt_idx = compute_cpt_index_v2(miss_parent_radices, miss_parent_indices, 
                                                   v, miss_values.size());
                if (cpt_idx >= 0 && cpt_idx < miss_cpt.size()) {
                    probs[v] = miss_cpt[cpt_idx];
                }
            }
        }
        
        // Normalize probabilities
        double prob_sum = 0;
        for (double p : probs) prob_sum += p;
        if (prob_sum > 0) {
            for (double& p : probs) p /= prob_sum;
        }
        
        // For each possible value of missing variable, add fractional counts
        for (int v = 0; v < miss_values.size(); v++) {
            // Create a complete record with missing value = v
            Record complete_rec = rec;
            complete_rec.values[miss_idx] = miss_values[v];
            complete_rec.missing_index = -1;
            
            double weight = probs[v];
            
            // Add weighted counts for all variables
            for (int i = 0; i < N; i++) {
                auto node = net.get_nth_node(i);
                vector<string> parents = node->get_Parents();
                vector<string> values = node->get_values();
                
                // Compute parent radices
                vector<int> parent_radices;
                for (const auto& pname : parents) {
                    auto pnode = net.search_node(pname);
                    parent_radices.push_back(pnode->get_nvalues());
                }
                
                // Get parent indices
                vector<int> parent_indices;
                bool valid_inner = true;
                for (const auto& pname : parents) {
                    int parent_idx = net.get_index(pname);
                    auto pnode = net.search_node(pname);
                    auto pvals = pnode->get_values();
                    
                    int val_idx = get_value_index(pvals, complete_rec.values[parent_idx]);
                    if (val_idx < 0) {
                        valid_inner = false;
                        break;
                    }
                    parent_indices.push_back(val_idx);
                }
                
                if (!valid_inner) continue;
                
                // Get child value
                int child_val_idx = get_value_index(values, complete_rec.values[i]);
                if (child_val_idx < 0) continue;
                
                // Update count with weight
                int cpt_idx = compute_cpt_index_v2(parent_radices, parent_indices, 
                                                   child_val_idx, values.size());
                if (cpt_idx >= 0 && cpt_idx < expected_counts[i].size()) {
                    expected_counts[i][cpt_idx] += weight;
                }
            }
        }
    }
}

// EM Algorithm - M-step: update parameters from expected counts
double em_m_step(network& net, const vector<vector<double>>& expected_counts,
                 const vector<vector<bool>>& learn_mask) {
    int N = net.netSize();
    double max_change = 0.0;
    
    for (int i = 0; i < N; i++) {
        auto node = net.get_nth_node(i);
        vector<string> parents = node->get_Parents();
        vector<string> values = node->get_values();
        vector<float> cpt = node->get_CPT();
        
        // Check if any parameters need learning
        bool needs_learning = false;
        for (bool mask_val : learn_mask[i]) {
            if (mask_val) {
                needs_learning = true;
                break;
            }
        }
        
        if (!needs_learning) continue;
        
        // Compute parent combinations
        int parent_combinations = 1;
        for (const auto& pname : parents) {
            auto pnode = net.search_node(pname);
            parent_combinations *= pnode->get_nvalues();
        }
        
        // Normalize counts to get probabilities
        vector<float> new_cpt = cpt;
        
        if (parents.empty()) {
            // Unconditional probability
            double sum = 0;
            for (int k = 0; k < values.size(); k++) {
                sum += expected_counts[i][k];
            }
            for (int k = 0; k < values.size(); k++) {
                if (learn_mask[i][k]) {
                    float new_val = expected_counts[i][k] / sum;
                    max_change = max(max_change, (double)abs(new_val - cpt[k]));
                    new_cpt[k] = new_val;
                }
            }
        } else {
            // Conditional probability
            for (int pc = 0; pc < parent_combinations; pc++) {
                double sum = 0;
                for (int k = 0; k < values.size(); k++) {
                    int idx = pc * values.size() + k;
                    sum += expected_counts[i][idx];
                }
                for (int k = 0; k < values.size(); k++) {
                    int idx = pc * values.size() + k;
                    if (learn_mask[i][idx]) {
                        float new_val = expected_counts[i][idx] / sum;
                        max_change = max(max_change, (double)abs(new_val - cpt[idx]));
                        new_cpt[idx] = new_val;
                    }
                }
            }
        }
        
        node->set_CPT(new_cpt);
    }
    
    return max_change;
}

// Main learning function
void learn_parameters(network& net, const vector<Record>& records, 
                     double convergence_threshold = 1e-7,
                     double alpha = 0.6) {
    
    cout << "Starting parameter learning..." << endl;
    cout << "Total records: " << records.size() << endl;
    
    int complete_count = 0;
    for (const auto& rec : records) {
        if (rec.missing_index < 0) complete_count++;
    }
    cout << "Complete records: " << complete_count << endl;
    cout << "Incomplete records: " << (records.size() - complete_count) << endl;
    
    // Identify which parameters need learning BEFORE any modification
    cout << "\nIdentifying parameters to learn..." << endl;
    vector<vector<bool>> learn_mask = identify_learning_params(net);
    
    // Initialize from all records (missing values treated uniformly)
    auto start_time = chrono::high_resolution_clock::now();
    cout << "Initializing parameters from all records (uniform distribution for missing values)..." << endl;
    initialize_cpt_from_complete(net, records, learn_mask, alpha);
    
    auto init_time = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(init_time - start_time);
    cout << "Initialization completed in " << duration.count() << " ms" << endl;
    
    // EM iterations
    int N = net.netSize();
    vector<vector<double>> expected_counts(N);
    
    int iter = 0;
    const double max_time_seconds = 114.0;  // Time limit in seconds
    
    while (true) {
        auto iter_start = chrono::high_resolution_clock::now();
        
        // Check time limit first
        auto elapsed = chrono::duration_cast<chrono::milliseconds>(iter_start - start_time);
        double elapsed_seconds = elapsed.count() / 1000.0;
        if (elapsed_seconds > max_time_seconds) {
            cout << "Time limit of " << max_time_seconds << " seconds reached after " 
                 << iter << " iterations" << endl;
            break;
        }
        
        // E-step
        em_e_step(net, records, expected_counts, alpha);
        
        // M-step
        double max_change = em_m_step(net, expected_counts, learn_mask);
        
        auto iter_end = chrono::high_resolution_clock::now();
        auto iter_duration = chrono::duration_cast<chrono::milliseconds>(iter_end - iter_start);
        
        iter++;
        cout << "Iteration " << iter << ": max parameter change = " 
             << fixed << setprecision(8) << max_change 
             << " (time: " << iter_duration.count() << " ms)" << endl;
        
        // Check convergence
        if (max_change < convergence_threshold) {
            cout << "Converged after " << iter << " iterations!" << endl;
            break;
        }
    }
    
    auto end_time = chrono::high_resolution_clock::now();
    auto total_duration = chrono::duration_cast<chrono::seconds>(end_time - start_time);
    cout << "\nTotal learning time: " << total_duration.count() << " seconds" << endl;
}

int main(int argc, char* argv[]) {
    if (argc < 3 || argc > 4) {
        cout << "Usage: " << argv[0] << " <network.bif> <data.dat> [output.bif]" << endl;
        return 1;
    }
    
    const char* network_file = argv[1];
    const char* data_file = argv[2];
    const char* output_file = (argc >= 4) ? argv[3] : "solved_hailfinder.bif";
    
    cout << "Reading Bayesian Network from " << network_file << "..." << endl;
    network BayesNet = read_network(network_file);
    
    if (BayesNet.netSize() == 0) {
        cout << "Failed to read network!" << endl;
        return 1;
    }
    
    cout << "Network loaded: " << BayesNet.netSize() << " nodes" << endl;
    
    cout << "\nReading data records from " << data_file << "..." << endl;
    vector<Record> records = read_records(data_file, BayesNet.netSize());
    
    if (records.empty()) {
        cout << "Failed to read records!" << endl;
        return 1;
    }
    
    cout << "Data loaded: " << records.size() << " records" << endl;
    
    // Learn parameters
    learn_parameters(BayesNet, records);
    
    // Write output
    cout << "\nWriting learned network to " << output_file << "..." << endl;
    write_network(output_file, BayesNet);
    
    cout << "\nDone!" << endl;
    
    return 0;
}
