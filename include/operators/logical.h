//
// Created by alex on 15/12/15.
//

#ifndef AUTODIFF_LOGICAL_H
#define AUTODIFF_LOGICAL_H
namespace metadiff {

    void throw_logical_error(NodeInVec parents){
        throw UnknownError(parents, "The logical operator recieved a gradient message.");
    }

    class LogicalBinary : public ElementwiseBinary{
    public:
        LogicalBinary(std::string const name,
                      GraphInPtr graph,
                      NodeInPtr parent1,
                      NodeInPtr parent2):
                ElementwiseBinary(name, graph, parent1, parent2)
        {};

        ad_value_type get_value_type(){
            return BOOLEAN;
        };

        ad_node_type get_node_type(){
            auto parent1_type = parent1.lock()->type;
            auto parent2_type = parent2.lock()->type;
            if(parent1_type == CONSTANT and parent2_type == CONSTANT){
                return CONSTANT;
            } else {
                return CONSTANT_DERIVED;
            }
        };

        void generate_gradients(size_t current, std::unordered_map<size_t, size_t> &messages) {
            auto graph = this->graph.lock();
            if (messages.find(current) != messages.end()) {
                throw_logical_error({parent1, parent2});
            }
            return;
        };
    };

    class LogicalUnary : public UnaryOperator{
    public:
        LogicalUnary(std::string const name,
                     GraphInPtr graph,
                     NodeInPtr parent):
                UnaryOperator(name, graph, parent)
        {};

        ad_value_type get_value_type(){
            return BOOLEAN;
        };

        ad_node_type get_node_type(){
            auto parent_type = parent.lock()->type;
            if(parent_type == CONSTANT){
                return CONSTANT;
            } else {
                return CONSTANT_DERIVED;
            }
        };

        void generate_gradients(size_t current, std::unordered_map<size_t, size_t> &messages) {
            auto graph = this->graph.lock();
            if (messages.find(current) != messages.end()) {
                throw_logical_error({parent});
            }
            return;
        };
    };

    class GreaterThan : public LogicalBinary {
    public:
        GreaterThan(GraphInPtr graph,
                    NodeInPtr parent1,
                    NodeInPtr parent2) :
                LogicalBinary("Gt", graph, parent1, parent2)
        {};
    };

    Node gt(Node node1, Node node2){
        auto graph = node1.graph.lock();
        auto arg1 = graph->nodes[node1.id];
        auto arg2 = graph->nodes[node2.id];
        auto op = std::make_shared<GreaterThan>(graph, arg1, arg2);
        return Node(graph, graph->derived_node(op).lock()->id);
    }

    Node operator>(Node node1, Node node2){
        return gt(node1, node2);
    }

    class GreaterThanOrEqual : public LogicalBinary {
    public:
        GreaterThanOrEqual(GraphInPtr graph,
                           NodeInPtr parent1,
                           NodeInPtr parent2) :
                LogicalBinary("Ge", graph, parent1, parent2)
        {};
    };

    Node gte(Node node1, Node node2){
        auto graph = node1.graph.lock();
        auto arg1 = graph->nodes[node1.id];
        auto arg2 = graph->nodes[node2.id];
        auto op = std::make_shared<GreaterThanOrEqual>(graph, arg1, arg2);
        return Node(graph, graph->derived_node(op).lock()->id);
    }

    Node operator>=(Node node1, Node node2){
        return gte(node1, node2);
    }

    class LessThan : public LogicalBinary {
    public:
        LessThan(GraphInPtr graph,
                 NodeInPtr parent1,
                 NodeInPtr parent2) :
                LogicalBinary("Lt", graph, parent1, parent2)
        {};
    };

    Node lt(Node node1, Node node2){
        auto graph = node1.graph.lock();
        auto arg1 = graph->nodes[node1.id];
        auto arg2 = graph->nodes[node2.id];
        auto op = std::make_shared<LessThan>(graph, arg1, arg2);
        return Node(graph, graph->derived_node(op).lock()->id);
    }

    Node operator<(Node node1, Node node2){
        return lt(node1, node2);
    }

    class LessThanOrEqual : public LogicalBinary {
    public:
        LessThanOrEqual(GraphInPtr graph,
                        NodeInPtr parent1,
                        NodeInPtr parent2) :
                LogicalBinary("Le", graph, parent1, parent2)
        {};
    };

    Node lte(Node node1, Node node2){
        auto graph = node1.graph.lock();
        auto arg1 = graph->nodes[node1.id];
        auto arg2 = graph->nodes[node2.id];
        auto op = std::make_shared<LessThanOrEqual>(graph, arg1, arg2);
        return Node(graph, graph->derived_node(op).lock()->id);
    }

    Node operator<=(Node node1, Node node2){
        return lte(node1, node2);
    }

    class Equals : public LogicalBinary {
    public:
        Equals(GraphInPtr graph,
               NodeInPtr parent1,
               NodeInPtr parent2) :
                LogicalBinary("Eq", graph, parent1, parent2)
        {};
    };

    Node eq(Node node1, Node node2){
        auto graph = node1.graph.lock();
        auto arg1 = graph->nodes[node1.id];
        auto arg2 = graph->nodes[node2.id];
        auto op = std::make_shared<Equals>(graph, arg1, arg2);
        return Node(graph, graph->derived_node(op).lock()->id);
    }

    Node operator==(Node node1, Node node2){
        return eq(node1, node2);
    }

    class NotEquals : public LogicalBinary {
    public:
        NotEquals(GraphInPtr graph,
                  NodeInPtr parent1,
                  NodeInPtr parent2) :
                LogicalBinary("Ne", graph, parent1, parent2)
        {};
    };

    Node neq(Node node1, Node node2){
        auto graph = node1.graph.lock();
        auto arg1 = graph->nodes[node1.id];
        auto arg2 = graph->nodes[node2.id];
        auto op = std::make_shared<NotEquals>(graph, arg1, arg2);
        return Node(graph, graph->derived_node(op).lock()->id);
    }

    Node operator!=(Node node1, Node node2){
        return neq(node1, node2);
    }

    class ApproximatelyEquals : public LogicalBinary {
    public:
        double tol;
        ApproximatelyEquals(GraphInPtr graph,
                            NodeInPtr parent1,
                            NodeInPtr parent2,
                            double tol) :
                LogicalBinary("ApproxEq", graph, parent1, parent2),
                tol(tol)
        {};
    };

    Node approx_eq(Node node1, Node node2, double tol=1e-9){
        auto graph = node1.graph.lock();
        auto arg1 = graph->nodes[node1.id];
        auto arg2 = graph->nodes[node2.id];
        auto op = std::make_shared<ApproximatelyEquals>(graph, arg1, arg2, tol);
        return Node(graph, graph->derived_node(op).lock()->id);
    }

    class ApproximatelyNotEquals : public LogicalBinary {
    public:
        double tol;
        ApproximatelyNotEquals(GraphInPtr graph,
                               NodeInPtr parent1,
                               NodeInPtr parent2,
                               double tol) :
                LogicalBinary("ApproxNe", graph, parent1, parent2),
                tol(tol)
        {};
    };

    Node approx_neq(Node node1, Node node2, double tol = 1e-9){
        auto graph = node1.graph.lock();
        auto arg1 = graph->nodes[node1.id];
        auto arg2 = graph->nodes[node2.id];
        auto op = std::make_shared<ApproximatelyNotEquals>(graph, arg1, arg2, tol);
        return Node(graph, graph->derived_node(op).lock()->id);
    }

    class And : public LogicalBinary {
    public:
        And(GraphInPtr graph,
            NodeInPtr parent1,
            NodeInPtr parent2) :
                LogicalBinary("And", graph, parent1, parent2)
        {
            if(parent1.lock()->v_type != BOOLEAN or parent2.lock()->v_type != BOOLEAN){
                throw UnknownError({parent1, parent2}, "Operator 'And' accepts only BOOLEAN inputs");
            }
        };
    };

    Node logical_and(Node node1, Node node2){
        auto graph = node1.graph.lock();
        auto arg1 = graph->nodes[node1.id];
        auto arg2 = graph->nodes[node2.id];
        auto op = std::make_shared<And>(graph, arg1, arg2);
        return Node(graph, graph->derived_node(op).lock()->id);
    }

    Node operator&&(Node node1, Node node2){
        return logical_and(node1, node2);
    }

    class Or : public LogicalBinary {
    public:
        Or(GraphInPtr graph,
           NodeInPtr parent1,
           NodeInPtr parent2) :
                LogicalBinary("Or", graph, parent1, parent2)
        {
            if(parent1.lock()->v_type != BOOLEAN or parent2.lock()->v_type != BOOLEAN){
                throw UnknownError({parent1, parent2}, "Operator 'Or' accepts only BOOLEAN inputs");
            }
        };
    };

    Node logical_or(Node node1, Node node2){
        auto graph = node1.graph.lock();
        auto arg1 = graph->nodes[node1.id];
        auto arg2 = graph->nodes[node2.id];
        auto op = std::make_shared<Or>(graph, arg1, arg2);
        return Node(graph, graph->derived_node(op).lock()->id);
    }

    Node operator||(Node node1, Node node2){
        return logical_or(node1, node2);
    }

    class ZeroElements : public LogicalUnary {
    public:
        ZeroElements(GraphInPtr graph,
                     NodeInPtr parent) :
                LogicalUnary("ZeroElem", graph, parent)
        {};
    };

    Node Node::zeros() {
        auto graph = this->graph.lock();
        auto arg = graph->nodes[this->id];
        auto op = std::make_shared<ZeroElements>(graph, arg);
        return Node(graph, graph->derived_node(op).lock()->id);
    }

    Node zero_elem(Node node){
        return node.zeros();
    }

    class NonZeroElements : public LogicalUnary {
    public:
        NonZeroElements(GraphInPtr graph,
                        NodeInPtr parent) :
                LogicalUnary("NonZeroElem", graph, parent)
        {};
    };

    Node Node::non_zeros(){
        auto graph = this->graph.lock();
        auto arg = graph->nodes[this->id];
        auto op = std::make_shared<NonZeroElements>(graph, arg);
        return Node(graph, graph->derived_node(op).lock()->id);
    }

    Node non_zero_elem(Node node){
        return node.non_zeros();
    }

    class IsNaN : public LogicalUnary {
    public:
        IsNaN(GraphInPtr graph,
              NodeInPtr parent) :
                LogicalUnary("IsNaN", graph, parent)
        {};
    };

    Node Node::is_nan(){
        auto graph = this->graph.lock();
        auto arg = graph->nodes[this->id];
        auto op = std::make_shared<IsNaN>(graph, arg);
        return Node(graph, graph->derived_node(op).lock()->id);
    }

    Node is_nan(Node node){
        return node.is_nan();
    }

    class IsInf : public LogicalUnary {
    public:
        IsInf(GraphInPtr graph,
              NodeInPtr parent) :
                LogicalUnary("IsInf", graph, parent)
        {};
    };

    Node Node::is_inf(){
        auto graph = this->graph.lock();
        auto arg = graph->nodes[this->id];
        auto op = std::make_shared<IsInf>(graph, arg);
        return Node(graph, graph->derived_node(op).lock()->id);
    }

    Node is_inf(Node node){
        return node.is_inf();
    }
}
#endif //AUTODIFF_LOGICAL_H
