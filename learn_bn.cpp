#define BN_LIB
#include "starter.cpp"
#include <algorithm>
#include <chrono>

using namespace std;

struct Record {
    vector<string> values;
    int missing_index;
    
    Record() : missing_index(-1) {}
};

string remove_quotes(string s) {
    if (s.length() >= 2 && s[0] == '"' && s[s.length()-1] == '"') {
        return s.substr(1, s.length()-2);
    }
    return s;
}

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

int get_value_index(const vector<string>& values, const string& value) {
    for (int i = 0; i < values.size(); i++) {
        if (values[i] == value) return i;
    }
    return -1;
}

int compute_cpt_index(network& net, const vector<string>& parents, 
                      const vector<string>& parent_values, int child_value_idx) {
    if (parents.empty()) {
        return child_value_idx;
    }
    
    int parent_config = 0;
    int multiplier = 1;
    
    for (int p = parents.size() - 1; p >= 0; p--) {
        auto parent_node = net.search_node(parents[p]);
        auto parent_vals = parent_node->get_values();
        
        int parent_val_idx = get_value_index(parent_vals, parent_values[p]);
        if (parent_val_idx < 0) return -1;
        
        parent_config += parent_val_idx * multiplier;
        multiplier *= parent_vals.size();
    }
    
    auto node = net.search_node(parents[0]);
    
    return -1;
}

int compute_cpt_index_v2(const vector<int>& parent_radices, 
                         const vector<int>& parent_indices, 
                         int child_value_idx, int child_nvalues) {
    if (parent_radices.empty()) {
        return child_value_idx;
    }
    
    int parent_config = 0;
    int multiplier = 1;
    
    for (int p = parent_radices.size() - 1; p >= 0; p--) {
        parent_config += parent_indices[p] * multiplier;
        multiplier *= parent_radices[p];
    }
    
    return parent_config * child_nvalues + child_value_idx;
}

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

void initialize_to_uniform(network& net, const vector<vector<bool>>& learn_mask) {
    int N = net.netSize();
    
    for (int i = 0; i < N; i++) {
        auto node = net.get_nth_node(i);
        auto parents = node->get_Parents();
        auto values = node->get_values();
        vector<float> cpt = node->get_CPT();
        
        int parent_combinations = 1;
        for (const auto& pname : parents) {
            parent_combinations *= net.search_node(pname)->get_nvalues();
        }
        
        for (int pc = 0; pc < parent_combinations; pc++) {
            bool needs_init = false;
            for (int k = 0; k < values.size(); k++) {
                int idx = pc * values.size() + k;
                if (cpt[idx] < 0) {
                    needs_init = true;
                    break;
                }
            }
            
            if (needs_init) {
                float uniform_prob = 1.0f / values.size();
                for (int k = 0; k < values.size(); k++) {
                    int idx = pc * values.size() + k;
                    if (cpt[idx] < 0) {
                        cpt[idx] = uniform_prob;
                    }
                }
            }
        }
        
        node->set_CPT(cpt);
    }
}

void initialize_from_complete_records(network& net, const vector<Record>& complete_records, 
                                      const vector<vector<bool>>& learn_mask, double alpha) {
    int N = net.netSize();
    
    cout << "  Initializing from " << complete_records.size() << " complete records..." << endl;
    
    for (int i = 0; i < N; i++) {
        auto node = net.get_nth_node(i);
        vector<string> parents = node->get_Parents();
        vector<string> values = node->get_values();
        vector<float> cpt = node->get_CPT();
        
        
        bool needs_learning = false;
        for (bool mask_val : learn_mask[i]) {
            if (mask_val) {
                needs_learning = true;
                break;
            }
        }
        
        if (!needs_learning) continue;
        
        
        vector<int> parent_radices;
        for (const auto& pname : parents) {
            auto pnode = net.search_node(pname);
            parent_radices.push_back(pnode->get_nvalues());
        }
        
        int parent_combinations = 1;
        for (int r : parent_radices) parent_combinations *= r;
        
        
        vector<double> counts(cpt.size(), alpha);
        
        
        for (const auto& rec : complete_records) {
            
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
            
            
            int child_val_idx = get_value_index(values, rec.values[i]);
            if (child_val_idx < 0) continue;
            
            
            int cpt_idx = compute_cpt_index_v2(parent_radices, parent_indices, 
                                               child_val_idx, values.size());
            if (cpt_idx >= 0 && cpt_idx < counts.size()) {
                counts[cpt_idx] += 1.0;
            }
        }
        
        
        vector<float> new_cpt = cpt;
        
        if (parents.empty()) {
            
            double sum = 0;
            for (int k = 0; k < values.size(); k++) {
                sum += counts[k];
            }
            
            const double epsilon = 1e-10;
            sum = max(sum, epsilon);
            
            for (int k = 0; k < values.size(); k++) {
                if (learn_mask[i][k]) {
                    new_cpt[k] = counts[k] / sum;
                }
            }
        } else {
            
            for (int pc = 0; pc < parent_combinations; pc++) {
                double sum = 0;
                for (int k = 0; k < values.size(); k++) {
                    int idx = pc * values.size() + k;
                    sum += counts[idx];
                }
                
                const double epsilon = 1e-10;
                sum = max(sum, epsilon);
                
                for (int k = 0; k < values.size(); k++) {
                    int idx = pc * values.size() + k;
                    if (learn_mask[i][idx]) {
                        new_cpt[idx] = counts[idx] / sum;
                    }
                }
            }
        }
        
        node->set_CPT(new_cpt);
    }
}


double get_parent_probability(network& net, const Record& rec, 
                              const string& parent_name, int parent_value_idx) {
    int parent_idx = net.get_index(parent_name);
    auto parent_node = net.search_node(parent_name);
    auto parent_parents = parent_node->get_Parents();
    auto parent_cpt = parent_node->get_CPT();
    
    
    if (parent_parents.empty()) {
        if (parent_value_idx >= 0 && parent_value_idx < parent_cpt.size()) {
            return parent_cpt[parent_value_idx];
        }
        return 1.0 / parent_node->get_nvalues(); 
    }
    
    
    vector<int> pp_indices;
    vector<int> pp_radices;
    bool valid = true;
    
    for (const auto& pp_name : parent_parents) {
        int pp_idx = net.get_index(pp_name);
        auto pp_node = net.search_node(pp_name);
        auto pp_values = pp_node->get_values();
        
        
        if (rec.missing_index == pp_idx) {
            
            
            return 1.0 / parent_node->get_nvalues();
        }
        
        int pp_val_idx = get_value_index(pp_values, rec.values[pp_idx]);
        if (pp_val_idx < 0) {
            valid = false;
            break;
        }
        pp_indices.push_back(pp_val_idx);
        pp_radices.push_back(pp_values.size());
    }
    
    if (!valid) {
        return 1.0 / parent_node->get_nvalues(); 
    }
    
    
    int cpt_idx = compute_cpt_index_v2(pp_radices, pp_indices, 
                                       parent_value_idx, parent_node->get_nvalues());
    
    if (cpt_idx >= 0 && cpt_idx < parent_cpt.size() && parent_cpt[cpt_idx] >= 0) {
        return parent_cpt[cpt_idx];
    }
    
    return 1.0 / parent_node->get_nvalues(); 
}



double compute_markov_blanket_probability(network& net, int missing_var_idx, int value_idx,
                                          const Record& record, const vector<string>& all_possible_values) {
    auto missing_node = net.get_nth_node(missing_var_idx);
    vector<string> parents = missing_node->get_Parents();
    vector<int> children = missing_node->get_children();
    
    double log_prob = 0.0;  
    
    
    vector<int> parent_val_indices;
    bool valid_parents = true;
    for (const auto& pname : parents) {
        int parent_idx = net.get_index(pname);
        auto pnode = net.search_node(pname);
        auto pvals = pnode->get_values();
        int val_idx = get_value_index(pvals, record.values[parent_idx]);
        if (val_idx < 0) {
            valid_parents = false;
            break;
        }
        parent_val_indices.push_back(val_idx);
    }
    
    if (valid_parents) {
        
        vector<int> parent_radices;
        for (const auto& pname : parents) {
            auto pnode = net.search_node(pname);
            parent_radices.push_back(pnode->get_nvalues());
        }
        
        int cpt_idx = compute_cpt_index_v2(parent_radices, parent_val_indices, 
                                           value_idx, all_possible_values.size());
        vector<float> cpt = missing_node->get_CPT();
        if (cpt_idx >= 0 && cpt_idx < cpt.size()) {
            double prob = cpt[cpt_idx];
            if (prob > 0) {
                log_prob += log(prob);
            } else {
                log_prob += log(1e-10); 
            }
        }
    }
    
    
    for (int child_idx : children) {
        auto child_node = net.get_nth_node(child_idx);
        vector<string> child_parents = child_node->get_Parents();
        vector<string> child_values = child_node->get_values();
        
        
        string child_observed_val = record.values[child_idx];
        int child_val_idx = get_value_index(child_values, child_observed_val);
        if (child_val_idx < 0) continue;
        
        
        vector<int> child_parent_radices;
        vector<int> child_parent_val_indices;
        bool valid_child_parents = true;
        
        for (const auto& cpname : child_parents) {
            int cp_idx = net.get_index(cpname);
            auto cpnode = net.search_node(cpname);
            auto cpvals = cpnode->get_values();
            child_parent_radices.push_back(cpvals.size());
            
            int cp_val_idx;
            if (cp_idx == missing_var_idx) {
                
                cp_val_idx = value_idx;
            } else {
                
                cp_val_idx = get_value_index(cpvals, record.values[cp_idx]);
                if (cp_val_idx < 0) {
                    valid_child_parents = false;
                    break;
                }
            }
            child_parent_val_indices.push_back(cp_val_idx);
        }
        
        if (valid_child_parents) {
            
            int child_cpt_idx = compute_cpt_index_v2(child_parent_radices, 
                                                     child_parent_val_indices,
                                                     child_val_idx, child_values.size());
            vector<float> child_cpt = child_node->get_CPT();
            if (child_cpt_idx >= 0 && child_cpt_idx < child_cpt.size()) {
                double child_prob = child_cpt[child_cpt_idx];
                if (child_prob > 0) {
                    log_prob += log(child_prob);
                } else {
                    log_prob += log(1e-10);
                }
            }
        }
    }
    
    return exp(log_prob);  
}


void em_e_step(network& net, const vector<Record>& records, 
               vector<vector<double>>& expected_counts, double alpha) {
    int N = net.netSize();
    
    
    for (int i = 0; i < N; i++) {
        auto node = net.get_nth_node(i);
        expected_counts[i].assign(node->get_CPT().size(), alpha);
    }
    
    
    for (const auto& rec : records) {
        if (rec.missing_index >= 0) continue; 
        
        for (int i = 0; i < N; i++) {
            auto node = net.get_nth_node(i);
            vector<string> parents = node->get_Parents();
            vector<string> values = node->get_values();
            
            
            vector<int> parent_radices;
            for (const auto& pname : parents) {
                auto pnode = net.search_node(pname);
                parent_radices.push_back(pnode->get_nvalues());
            }
            
            
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
            
            
            int child_val_idx = get_value_index(values, rec.values[i]);
            if (child_val_idx < 0) continue;
            
            
            int cpt_idx = compute_cpt_index_v2(parent_radices, parent_indices, 
                                               child_val_idx, values.size());
            if (cpt_idx >= 0 && cpt_idx < expected_counts[i].size()) {
                expected_counts[i][cpt_idx] += 1.0;
            }
        }
    }
    
    
    for (const auto& rec : records) {
        if (rec.missing_index < 0) continue; 
        
        int miss_idx = rec.missing_index;
        auto miss_node = net.get_nth_node(miss_idx);
        auto miss_values = miss_node->get_values();
        
        
        
        vector<double> probs(miss_values.size(), 0.0);
        
        for (int v = 0; v < miss_values.size(); v++) {
            probs[v] = compute_markov_blanket_probability(net, miss_idx, v, rec, miss_values);
        }
        
        
        double prob_sum = 0;
        for (double p : probs) prob_sum += p;
        if (prob_sum > 0) {
            for (double& p : probs) p /= prob_sum;
        } else {
            
            for (double& p : probs) p = 1.0 / miss_values.size();
        }
        
        
        for (int v = 0; v < miss_values.size(); v++) {
            
            Record complete_rec = rec;
            complete_rec.values[miss_idx] = miss_values[v];
            complete_rec.missing_index = -1;
            
            double weight = probs[v];
            
            
            for (int i = 0; i < N; i++) {
                auto node = net.get_nth_node(i);
                vector<string> parents = node->get_Parents();
                vector<string> values = node->get_values();
                
                
                vector<int> parent_radices;
                for (const auto& pname : parents) {
                    auto pnode = net.search_node(pname);
                    parent_radices.push_back(pnode->get_nvalues());
                }
                
                
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
                
                
                int child_val_idx = get_value_index(values, complete_rec.values[i]);
                if (child_val_idx < 0) continue;
                
                
                int cpt_idx = compute_cpt_index_v2(parent_radices, parent_indices, 
                                                   child_val_idx, values.size());
                if (cpt_idx >= 0 && cpt_idx < expected_counts[i].size()) {
                    expected_counts[i][cpt_idx] += weight;
                }
            }
        }
    }
}


double em_m_step(network& net, const vector<vector<double>>& expected_counts,
                 const vector<vector<bool>>& learn_mask) {
    int N = net.netSize();
    double max_change = 0.0;
    
    for (int i = 0; i < N; i++) {
        auto node = net.get_nth_node(i);
        vector<string> parents = node->get_Parents();
        vector<string> values = node->get_values();
        vector<float> cpt = node->get_CPT();
        
        
        bool needs_learning = false;
        for (bool mask_val : learn_mask[i]) {
            if (mask_val) {
                needs_learning = true;
                break;
            }
        }
        
        if (!needs_learning) continue;
        
        
        int parent_combinations = 1;
        for (const auto& pname : parents) {
            auto pnode = net.search_node(pname);
            parent_combinations *= pnode->get_nvalues();
        }
        
        
        vector<float> new_cpt = cpt;
        
        if (parents.empty()) {
            
            double sum = 0;
            for (int k = 0; k < values.size(); k++) {
                sum += expected_counts[i][k];
            }

            const double epsilon = 1e-10;
            sum = max(sum, epsilon);

            for (int k = 0; k < values.size(); k++) {
                if (learn_mask[i][k]) {
                    float new_val = expected_counts[i][k] / sum;
                    max_change = max(max_change, (double)abs(new_val - cpt[k]));
                    new_cpt[k] = new_val;
                }
            }
        } else {
            
            for (int pc = 0; pc < parent_combinations; pc++) {
                double sum = 0;
                for (int k = 0; k < values.size(); k++) {
                    int idx = pc * values.size() + k;
                    sum += expected_counts[i][idx];
                }

                const double epsilon = 1e-10;
                sum = max(sum, epsilon);

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


void learn_parameters(network& net, const vector<Record>& records, 
                     double convergence_threshold = 1e-8,
                     double alpha = 0.5) {
    
    
    vector<Record> complete_records;
    vector<Record> incomplete_records;
    
    for (const auto& rec : records) {
        if (rec.missing_index < 0) {
            complete_records.push_back(rec);
        } else {
            incomplete_records.push_back(rec);
        }
    }
    
    vector<vector<bool>> learn_mask = identify_learning_params(net);
    
    auto start_time = chrono::high_resolution_clock::now();
    
    if (!complete_records.empty()) {
        
        initialize_to_uniform(net, learn_mask);
        
        
        initialize_from_complete_records(net, complete_records, learn_mask, alpha);
    } else {
        initialize_to_uniform(net, learn_mask);
    }
    
    auto phase1_time = chrono::high_resolution_clock::now();
    auto phase1_duration = chrono::duration_cast<chrono::milliseconds>(phase1_time - start_time);
    
    int N = net.netSize();
    vector<vector<double>> expected_counts(N);
    
    const int min_iterations = 15;
    const double max_time_seconds = 110.0;  
    
    int iter = 0;
    
    while (true) {
        auto iter_start = chrono::high_resolution_clock::now();
        
        
        auto elapsed = chrono::duration_cast<chrono::milliseconds>(iter_start - start_time);
        double elapsed_seconds = elapsed.count() / 1000.0;
        if (elapsed_seconds > max_time_seconds) {
            cout << "Time limit of " << max_time_seconds << " seconds reached after " 
                 << iter << " iterations" << endl;
            break;
        }      
        
        em_e_step(net, records, expected_counts, alpha);
          
        double max_change = em_m_step(net, expected_counts, learn_mask);
        
        auto iter_end = chrono::high_resolution_clock::now();
        auto iter_duration = chrono::duration_cast<chrono::milliseconds>(iter_end - iter_start);
        
        iter++;
        
        if (iter <= 5 || iter % 5 == 0 || max_change < convergence_threshold) {
            cout << "Iteration " << iter << ": max parameter change = " 
                 << fixed << setprecision(8) << max_change 
                 << " (time: " << iter_duration.count() << " ms)" << endl;
        }
        
        
        if (iter >= min_iterations && max_change < convergence_threshold) {
            cout << "Converged after " << iter << " iterations!" << endl;
            break;
        }
    }
    
    auto end_time = chrono::high_resolution_clock::now();
    auto total_duration = chrono::duration_cast<chrono::seconds>(end_time - start_time);
   
}

int main(int argc, char* argv[]) {
    if (argc < 3 || argc > 4) {
        cout << "Usage: " << argv[0] << " <network.bif> <data.dat> [output.bif]" << endl;
        return 1;
    }
    
    const char* network_file = argv[1];
    const char* data_file = argv[2];
    const char* output_file = (argc >= 4) ? argv[3] : "solved_hailfinder.bif";   
   
    network BayesNet = read_network(network_file);
    
    if (BayesNet.netSize() == 0) {
        cout << "Failed to read network!" << endl;
        return 1;
    }
    
    vector<Record> records = read_records(data_file, BayesNet.netSize());
    
    if (records.empty()) {
        return 1;
    }   
    
    learn_parameters(BayesNet, records);
    
    cout << "\nWriting learned network to " << output_file << "..." << endl;
    write_network(output_file, BayesNet);
    
    return 0;
}