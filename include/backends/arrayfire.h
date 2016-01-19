//
// Created by alex on 18/12/15.
//

#ifndef AUTODIFF_BACKENDS_ARRAYFIRE_H
#define AUTODIFF_BACKENDS_ARRAYFIRE_H

#include <fstream>
namespace metadiff{

    class ArrayfireBackend: public FunctionBackend<af::array>{
    public:
        std::string include_path;
        std::string lib_path;
        ArrayfireBackend(std::string include_path,
                         std::string lib_path):
                include_path(include_path),
                lib_path(lib_path)
        {};

        ArrayfireBackend(){
            // Create backend and compile function
            const char *AF_PATH = getenv("AF_PATH") ? getenv("AF_PATH") : "/opt/arrayfire-3";
            include_path = std::string(AF_PATH) + "/include";
            lib_path = std::string(AF_PATH) + "/lib";
        };

        ArrayfireBackend(std::string AF_PATH){
            // Create backend and compile function
            include_path = AF_PATH + "/include";
            lib_path = AF_PATH + "/lib";
        };

        void compile_file(std::string file_name, std::string dll_name){
            std::string command = "MKL_NUM_THREADS=4 g++ -O3 -Wall -shared -fPIC -std=c++11 -laf ";
            command += "-Werror=return-type -Wno-unused-variable -Wno-narrowing ";
            command += " -I" + include_path;
            command += " -L" + lib_path;
//            command += " -I./";
            command += " -o " + dll_name + " " + file_name;
            std::cout << "Command: " << command << std::endl;
            std::cout << "Compilation response: " << system(command.c_str()) << std::endl;
            return;
        }

        void generate_source(std::string file_name, Graph graph,
                             std::vector<Node> inputs,
                             std::vector<Node> targets,
                             Updates& updates) {
            std::ofstream f;
            f.open(file_name);
            std::string tabs = "";
            // Disclaimer
            f << "// Auto generated by Metadiff\n// Please do not edit\n\n";
            // Includes
            f <<    "#include \"vector\"\n"
                    "#include \"memory\"\n"
                    "#include <exception>\n"
                    "#include <arrayfire.h>\n";
            f << "\n";
            // Write the interface definitions
            this->write_interface(f);

            f << "void print_mem_info(std::string name){\n"
                    "    size_t alloc_bytes,alloc_buffers,lock_bytes,lock_buffers;\n"
                    "    af::deviceMemInfo(&alloc_bytes,&alloc_buffers,&lock_bytes,&lock_buffers);\n"
                    "    std::cout << \"Memory info\" << name << std::endl;\n"
                    "    std::cout << \"Allocated: \" << alloc_bytes / 1024 << \" KB\" << std::endl;\n"
                    "    std::cout << \"Buffers allocated: \" << alloc_buffers << std::endl;\n"
                    "    std::cout << \"In use: \" << lock_bytes / 1024 << \" KB\" << std::endl;\n"
                    "    std::cout << \"Buffers in use: \" << lock_buffers << std::endl;\n"
                    "    return;\n"
                    "};\n\n";

            f << "inline af::array softplus(af::array input, int threshold) {\n"
                    "            af::array result = af::log1p(af::exp(input));\n"
                    "            af::replace(result, input < threshold, input);\n"
                    "            return result;\n"
                    "        }\n";

            f << "\n";
            f << "extern \"C\" std::vector<af::array> eval_func(std::vector<af::array>& inputs, std::vector<SharedPtr>& shared_vars){\n";
            f << "\t// Set up automatic broadcasting\n";
            f << "\taf::gforSet(true);\n";
            // Get ancestors mask
            graph->add_temporary_updates(updates);
            auto ancestor_mask = graph->get_ancestors_mask(targets);
            // Check that all inputs required are given
            for(int i=0;i<ancestor_mask.size();i++){
                if(ancestor_mask[i] and graph->nodes[i]->type == INPUT){
                    for(int j=0;j<=inputs.size();j++){
                        if(j == inputs.size()){
                            throw MissingRequiredInput(targets, i);
                        }
                        if(inputs[j].ptr->id == i){
                            break;
                        }
                    }
                }
            }

            std::vector<std::string> expression_table(graph->nodes.size(), "WTF");

            // Expressions for all inputs
            for(int i=0;i<inputs.size();i++){
                expression_table[inputs[i].ptr->id] = "inputs[" + std::to_string(i) + "]";
                ancestor_mask[inputs[i].ptr->id] = false;
            }
            // Expressions for all shared variables
            for(int i=0;i<graph->nodes.size();i++){
                Node node = graph->nodes[i];
                if(node.ptr->type == SHARED_INPUT) {
                    expression_table[i] = "shared_vars[" + std::to_string(node.ptr->shared->id) + "]->value";
                    ancestor_mask[i] = false;
                }
            }

//            // Calculate all of the symbolic integers
//            calculate_symbolics(f, graph, inputs);
//            // Validate input shapes
//            validate_input_shapes(f, graph, inputs);

            // Calculate all of the other nodes
            f << "\n\t// Calculate all of the computation nodes\n";
            for(size_t i=0;i<ancestor_mask.size();i++){
                if(ancestor_mask[i]){
                    std::string expression = calculate_node(graph, i, expression_table);
                    if(graph->nodes[i]->execution.inlined or graph->nodes[i]->op->name == "Broadcast"){
                        expression_table[i] = expression;
                    } else {
//                        f << "\tstd::cout << \"Evaluating: \" << " << i << " << std::endl;\n";
                        if(graph->nodes[i]->type == CONSTANT and Node(graph->nodes[i]).is_scalar()){
                            f << "\tfloat ";
                        }
                        else{
                            f << "\taf::array ";
                        }
                        f << "node_" << i << " = " << expression << ";\n";
                        expression_table[i] = "node_" + std::to_string(i);
                    }
                }
            }

            // Disable the automatic broadcasting
            f << "\taf::gforSet(false);";

            // Update all of the shared_variables
            f << "\n\t// Update all shared variables\n";
            for(int i=0;i<graph->nodes.size();i++){
                if(graph->nodes[i]->type == UPDATE){
//                    auto shared_id = graph->nodes[i]->op->get_arguments()[0].ptr->shared->id;
//                    auto update_id = graph->nodes[i]->op->get_parents()[0].ptr->id;
//                    f << "\tshared_vars[" << shared_id << "]->value = " << expression_table[update_id] << ";\n";
                    print_update_node(f, graph->nodes[i], expression_table);
                }
            }
            graph->clear_temporary_updates();
            // Write all of the output nodes as the result
            f << "\n\t// Write all of the output nodes in correct order\n";
            f << "\treturn {";
            for(int i=0;i<targets.size();i++){
                if(i < targets.size() - 1){
                    f << expression_table[targets[i].ptr->id] << ", ";
                } else {
                    f << expression_table[targets[i].ptr->id] << "};\n";
                }
            }
            f << "}\n";
            f.close();
        }

        void print_update_node(std::ofstream& f, Node node, std::vector<std::string>& expression_table){
//            f << "\tstd::cout << \"Updating node \" << " << node.ptr->id << " << std::endl;\n";
            size_t shared_id = node.ptr->op->get_arguments()[0].ptr->shared->id;
            Node update =  node.ptr->op->get_parents()[0];

            if(node.ptr->op->get_parents()[0].ptr->execution.inlined){
                if(update.ptr->op->name == "Mul"){
                    NodeVec parents = update.ptr->op->get_parents();
                    int index = -1;
                    bool all_div = true;
                    for(int j=0;j<parents.size();j++){
                        if(parents[j].ptr->type == SHARED_INPUT){
                            if(parents[j].ptr->shared->id == shared_id){
                                index = j;
                            } else {
                                all_div = false;
                            }
                        } else if(parents[j].ptr->op->name != "Div"){
                            all_div = false;
                        }
                    }
                    if(index == -1){
                        f << "\tshared_vars[" << shared_id << "]->value = " << expression_table[update.ptr->id] << ";\n";
                    } else if (all_div){
                        f << "\tshared_vars[" << shared_id << "]->value /= ";
                        bool first = true;
                        for (int j = 0; j < parents.size(); j++) {
                            if (j != index){
                                if(first){
                                    f << expression_table[parents[j].ptr->op->get_parents()[0].ptr->id];
                                    first = false;
                                } else {
                                    f << " * " << expression_table[parents[j].ptr->op->get_parents()[0].ptr->id];
                                }
                            }
                        }
                        f << ";\n";
                    } else {
                        f << "\tshared_vars[" << shared_id << "]->value *= ";
                        bool first = true;
                        for (int j = 0; j < parents.size(); j++) {
                            if (j != index){
                                if(first){
                                    f << expression_table[parents[j].ptr->op->get_parents()[0].ptr->id];
                                    first = false;
                                } else if(parents[j].ptr->op->name == "Div"){
                                    f << " / " + expression_table[parents[j].ptr->op->get_parents()[0].ptr->id];
                                } else {
                                    f << " * " + expression_table[parents[j].ptr->id];
                                }
                            }
                        }
                        f << ";\n";
                    }
                } else if(update.ptr->op->name == "Add"){
                    NodeVec parents = update.ptr->op->get_parents();
                    int index = -1;
                    bool all_neg = true;
                    for(int j=0;j<parents.size();j++){
                        if(parents[j].ptr->type == SHARED_INPUT){
                            if(parents[j].ptr->shared->id == shared_id){
                                index = j;
                            } else {
                                all_neg = false;
                            }
                        } else if(parents[j].ptr->op->name != "Neg"){
                            all_neg = false;
                        }
                    }
                    if(index == -1){
                        f << "\tshared_vars[" << shared_id << "]->value = " << expression_table[update.ptr->id] << ";\n";
                    } else if (all_neg){
                        f << "\tshared_vars[" << shared_id << "]->value -= ";
                        bool first = true;
                        for (int j = 0; j < parents.size(); j++) {
                            if (j != index){
                                if(first){
                                    f << expression_table[parents[j].ptr->op->get_parents()[0].ptr->id];
                                    first = false;
                                } else {
                                    f << " + " << expression_table[parents[j].ptr->op->get_parents()[0].ptr->id];
                                }
                            }
                        }
                        f << ";\n";
                    } else {
                        f << "\tshared_vars[" << shared_id << "]->value += ";
                        bool first = true;
                        for (int j = 0; j < parents.size(); j++) {
                            if (j != index){
                                if(first){
                                    f << expression_table[parents[j].ptr->op->get_parents()[0].ptr->id];
                                    first = false;
                                } else if(parents[j].ptr->op->name == "Neg"){
                                    f << " - " + expression_table[parents[j].ptr->op->get_parents()[0].ptr->id];
                                } else {
                                    f << " + " + expression_table[parents[j].ptr->id];
                                }
                            }
                        }
                        f << ";\n";
                    }
                } else {
                    f << "\tshared_vars[" << shared_id << "]->value = " << expression_table[update.ptr->id] << ";\n";
                }
            } else {
                f << "\tshared_vars[" << shared_id << "]->value = " << expression_table[update.ptr->id] << ";\n";
            }
        }

//        void calculate_symbolics(std::ofstream& f, Graph graph, std::vector<Node> inputs){
//            f << "\n\t// Set all of the symbolic variables\n";
//            for(size_t i=0;i<graph->sym_integer_count;i++){
//                SymInt variable = SymInt::variable(i);
//                f << "\tint " << variable << " = ";
//                bool done = false;
//                for(int j=0;j<inputs.size();j++){
//                    auto shape = graph->nodes[inputs[j].ptr->id]->shape;
//                    for(int s=0;s<4;s++){
//                        if(shape[s] == variable){
//                            f << "node_" << inputs[j].ptr->id << ".dims(" << s << ")";
//                            done = true;
//                            break;
//                        }
//                    }
//                    if(done){
//                        break;
//                    }
//                }
//                f << ";\n";
//            }
//        }
//
//        void validate_input_shapes(std::ofstream& f, Graph graph, std::vector<Node> inputs){
//            f <<"\n\t// Verify input sizes are correct\n";
//            for(int i=0;i<inputs.size();i++){
//                auto node = graph->nodes[i];
//                f << "\tsize_t node_" << node->id << "_expected_shape[4]{";
//                for(int j=0;j<4;j++){
//                    f << node->shape[j].to_string_with_star();
//                    if(j<3){
//                        f << ", ";
//                    }
//                }
//                f << "};\n";
//                f << "\tsize_t node_" << node->id << "_actual_shape[4]{";
//                for(int j=0;j<4;j++){
//                    f << "node_" << node->id << ".dims(" << j << ")";
//                    if(j<3){
//                        f << ", ";
//                    }
//                }
//                f << "};\n";
//                f << "\tif(node_" << node->id << "_expected_shape[0] != node_" << node->id << "_actual_shape[0]\n"
//                        "\t\tor node_" << node->id << "_expected_shape[1] != node_" << node->id << "_actual_shape[1]\n"
//                        "\t\tor node_" << node->id << "_expected_shape[2] != node_" << node->id << "_actual_shape[2]\n"
//                        "\t\tor node_" << node->id << "_expected_shape[3] != node_" << node->id << "_actual_shape[3]){\n"
//                        "\t\t throw InvalidInputShape(" << node->id <<
//                ", node_" << node->id << "_expected_shape, node_" << node->id << "_actual_shape);\n"
//                        "\t}\n";
//            }
//        }

        static std::string calculate_node(Graph graph, size_t id,
                                          std::vector<std::string>& expression_table){
            auto node = graph->nodes[id];
            auto op_name = node->op->name;
            auto parents = node->op->get_parents();
            auto args = node->op->get_arguments();
            auto children = node->children;
            if(node->type == UPDATE or node->type == INPUT) {
                throw 22;
            }
            if(node->type == CONSTANT and
                    (op_name == "Zeros" or op_name == "Ones" or op_name == "Value")){
                return std::to_string(node->op->get_scalar_value());
            }
            if (op_name == "Broadcast") {
                bool not_supported = false;
                for (int i = 0; i < children.size(); i++) {
                    auto name = children[i].ptr->op->name;
                    if (name != "Add" and name != "Mul"
                        and name != "Neg" and name != "Div") {
                        not_supported = true;
                        break;
                    }
                }
                if (not_supported) {
                    std::string expression = "af::tile(" + expression_table[parents[0].ptr->id] + ", ";
                    for (int i = 0; i < 4; i++){
                        if (node->shape[i] != parents[0].ptr->shape[i]) {
                            expression += node->shape[i].to_string_with_star();
                        } else {
                            expression += "1";
                        }
                        if (i < 3) {
                            expression += ", ";
                        }
                    }
                    return expression + ")";
                } else {
                    return expression_table[parents[0].ptr->id];
                }
            }
            if (op_name == "Add") {
                std::string expression = expression_table[parents[0].ptr->id];
                for (int i = 1; i < parents.size(); i++) {
                    if(parents[i].ptr->op->name == "Neg"){
                        expression += " - " + expression_table[parents[i].ptr->op->get_parents()[0].ptr->id];
                    } else {
                        expression += " + " + expression_table[parents[i].ptr->id];
                    }
                }
                return "(" + expression + ")";
            }
            if (op_name == "Neg") {
                return "(-" + expression_table[parents[0].ptr->id] + ")";
            }
            if (op_name == "Mul") {
                std::string expression = expression_table[parents[0].ptr->id];
                for (int i = 1; i < parents.size(); i++) {
                    if(parents[i].ptr->op->name == "Div"){
                        expression += " / " + expression_table[parents[i].ptr->op->get_parents()[0].ptr->id];
                    } else {
                        expression += " * " + expression_table[parents[i].ptr->id];
                    }
                }
                return expression;
            }
            if (op_name == "Div") {
                return "(1.0/" + expression_table[parents[0].ptr->id] + ")";
            }
            if (op_name == "Sum") {
                auto axes = dynamic_cast<Sum *>(node->op.get())->axes;
                if (Node(node).is_scalar()) {
                    return "af::sum(af::flat(" + expression_table[parents[0].ptr->id] + "))";
                } else {
                    std::string expression = expression_table[parents[0].ptr->id];
                    for (size_t i = 0; i < axes.size(); i++) {
                        expression = "af::sum(" + expression + ", " + std::to_string(i) + ")";
                    }
                    return expression;
                }
            }
            if (op_name == "Square") {
                return expression_table[parents[0].ptr->id] + " * " + expression_table[parents[0].ptr->id];
            }
            if (op_name == "Const") {
                return expression_table[parents[0].ptr->id];
            }
            if (op_name == "Gt") {
                return expression_table[parents[0].ptr->id] + " > " + expression_table[parents[1].ptr->id];
            }
            if (op_name == "Ge") {
                return expression_table[parents[0].ptr->id] + " >= " + expression_table[parents[1].ptr->id];
            }
            if (op_name == "Lt") {
                return expression_table[parents[0].ptr->id] + " < " + expression_table[parents[1].ptr->id];
            }
            if (op_name == "Lte") {
                return expression_table[parents[0].ptr->id] + " <= " + expression_table[parents[1].ptr->id];
            }
            if (op_name == "Eq") {
                return expression_table[parents[0].ptr->id] + " == " + expression_table[parents[1].ptr->id];
            }
            if (op_name == "Neq") {
                return expression_table[parents[0].ptr->id] + " != " + expression_table[parents[1].ptr->id];
            }
            if (op_name == "ApproxEq") {
                // TODO
                return "WTF";
            }
            if (op_name == "ApproxNe") {
                return "WTF";
            }
            if (op_name == "And") {
                return expression_table[parents[0].ptr->id] + " && " + expression_table[parents[1].ptr->id];
            }
            if (op_name == "Or") {
                return expression_table[parents[0].ptr->id] + " || " + expression_table[parents[1].ptr->id];
            }
            if (op_name == "ZeroElem") {
                return "af::iszero(" + expression_table[parents[0].ptr->id] + ")";
            }
            if (op_name == "NoneZeroElem") {
                return "!af::iszero(" + expression_table[parents[0].ptr->id] + ")";
            }
            if (op_name == "IsNaN") {
                return "af::isNaN(" + expression_table[parents[0].ptr->id] + ")";
            }
            if (op_name == "IsInf") {
                return "af::isInf(" + expression_table[parents[0].ptr->id] + ")";
            }
            if(op_name == "Select") {
                return "af::select(" + expression_table[args[0].ptr->id] + ", " +
                       expression_table[parents[0].ptr->id] + ", " +
                       expression_table[parents[1].ptr->id] + ")";
            }
            if (op_name == "Exp") {
                return "af::exp(" + expression_table[parents[0].ptr->id] + ")";
            }
            if (op_name == "Log") {
                return "af::log(" + expression_table[parents[0].ptr->id] + ")";
            }
            if (op_name == "Softplus") {
                size_t threshold = dynamic_cast<Softplus*>(node->op.get())->threshold;
                return "softplus(" + expression_table[parents[0].ptr->id] + ", " + std::to_string(threshold) + ")";
            }
            if (op_name == "Abs") {
                return "af::abs(" + expression_table[parents[0].ptr->id] + ")";
            }
            if (op_name == "Sigmoid") {
                return "1.0 / (1.0 + af::exp(-" + expression_table[parents[0].ptr->id] + "))";
            }
            if (op_name == "Sin") {
                return "af::sin(" + expression_table[parents[0].ptr->id] + ")";
            }
            if (op_name == "Cos") {
                return "af::cos(" + expression_table[parents[0].ptr->id] + ")";
            }
            if (op_name == "Tan") {
                return "af::tan(" + expression_table[parents[0].ptr->id] + ")";
            }
            if (op_name == "Sinh") {
                return "af::sinh(" + expression_table[parents[0].ptr->id] + ")";
            }
            if (op_name == "Cosh") {
                return "af::cosh(" + expression_table[parents[0].ptr->id] + ")";
            }
            if (op_name == "Tanh") {
                return "af::tanh(" + expression_table[parents[0].ptr->id] + ")";
            }
            if (op_name == "Pow") {
                return "af::pow(" + expression_table[parents[0].ptr->id] + ")";
            }
            if (op_name == "Transpose") {
                return "af::transpose(" + expression_table[parents[0].ptr->id] + ")";
            }
            if (op_name == "MatrixMul") {
                if(parents.size() > 2){
                    throw "Currently only matmul of 2 parents is supported";
                }
                std::string p0;
                std::string flag0 = "AF_MAT_NONE";
                std::string p1;
                std::string flag1 = "AF_MAT_NONE";
                std::string expr;
                if(parents[0].ptr->op->name == "Transpose"){
                    p0 = expression_table[parents[0].ptr->op->get_parents()[0].ptr->id];
                    flag0 = "AF_MAT_TRANS";
                } else {
                    p0 =  expression_table[parents[0].ptr->id];
                }
                if(parents[1].ptr->op->name == "Transpose"){
                    p1 = expression_table[parents[1].ptr->op->get_parents()[0].ptr->id];
                    flag1 = "AF_MAT_TRANS";
                } else {
                    p1 =  expression_table[parents[1].ptr->id];
                }
                return "af::matmul(" + p0 + ", " + p1 + ", " + flag0 + ", " + flag1 + ")";
            }
            if (op_name == "MatrixInv") {
                return "af::inverse(" + expression_table[parents[0].ptr->id] + ")";
            }
            if (op_name == "Det") {
                return "af::det(" + expression_table[parents[0].ptr->id] + ")";
            }
            if (op_name == "Logdet") {
                return "af::log(af::det(" + expression_table[parents[0].ptr->id] + "))";
            }
            if (op_name == "Trace") {
                return "af::sum(af::diag(" + expression_table[parents[0].ptr->id] + "))";
            }
            if (op_name == "Diag") {
                return "af::diag(" + expression_table[parents[0].ptr->id] + ", 0, " +
                       std::to_string(node->shape[1] == 1) + ")";
            }
            if (op_name == "Reshape") {
                std::string expression = "af::moddims(" + expression_table[parents[0].ptr->id] + ", ";
                for (int i = 0; i < 4; i++) {
                    expression += node->shape[i].to_string_with_star();
                    if (i < 3) {
                        expression += ", ";
                    }
                }
                return expression + ")";
            }
            if (op_name == "Reorder") {
                std::string expression = "af::reorder(" + expression_table[parents[0].ptr->id] + ", ";
                auto order = dynamic_cast<Reorder *>(node->op.get())->order;
                for (int i = 0; i < 4; i++) {
                    expression += order[i];
                    if (i < 3) {
                        expression += ", ";
                    }
                }
                return expression + ")";
            }
            if (op_name == "BinCrossEntropyLogit") {
                std::string p = expression_table[parents[0].ptr->id];
                std::string sfx = expression_table[args[0].ptr->id];
                std::string sfmx = expression_table[args[1].ptr->id];
                return p + " * (" + sfmx + " - " + sfx + ") + " + sfx;
            }
            if (op_name == "MaxAndArgMax") {
                return "WTF";
            }
            if (op_name == "SortAndArgSort") {
                return "WTF";
            }
            return "WTF2";
        }

        void print_shape_esception(std::ofstream& f){
            f << "class InvalidInputShape: public std::exception{\n"
                    "    public:\n"
                    "        size_t id;\n"
                    "        af::dim4 expected;\n"
                    "        af::dim4 given;\n"
                    "        std::string msg;\n"
                    "        InvalidInputShape(size_t id,\n"
                    "                          af::dim4 expected,\n"
                    "                          af::dim4 given):\n"
                    "                id(id),\n"
                    "                expected(expected),\n"
                    "                given(given)\n"
                    "        {\n"
                    "            msg = \"The input node with id \" + std::to_string(id) + \" provided has incorrect shape.\\n\" +\n"
                    "                  \"Expected:\" + std::to_string(expected[0]) + \", \" + std::to_string(expected[1]) + \", \"\n"
                    "                  + std::to_string(expected[2]) + \", \" + std::to_string(expected[3]) + \", \" +\"\\n\" +\n"
                    "                  \"Given:   \" + std::to_string(given[0]) + \", \" + std::to_string(given[1]) + \", \"\n"
                    "                  + std::to_string(given[2]) + \", \" + std::to_string(given[3]) + \", \" +\"\\n\";\n"
                    "        };\n"
                    "\n"
                    "        const char* what() const throw(){\n"
                    "            return msg.c_str();\n"
                    "        }\n"
                    "    };\n\n";
        }
    };
}

#endif //AUTODIFF_BACKENDS_ARRAYFIRE_H
