require "expression_tree"

module Groonga
  class ExpressionTreeBuilder
    RELATION_OPERATORS = [
      Operator::MATCH,
      Operator::NEAR,
      Operator::NEAR2,
      Operator::SIMILAR,
      Operator::PREFIX,
      Operator::SUFFIX,
      Operator::EQUAL,
      Operator::NOT_EQUAL,
      Operator::LESS,
      Operator::GREATER,
      Operator::LESS_EQUAL,
      Operator::GREATER_EQUAL,
      Operator::GEO_WITHINP5,
      Operator::GEO_WITHINP6,
      Operator::GEO_WITHINP8,
      Operator::TERM_EXTRACT,
      Operator::REGEXP,
      Operator::FUZZY,
    ]

    ARITHMETIC_OPERATORS = [
      Operator::BITWISE_OR,
      Operator::BITWISE_XOR,
      Operator::BITWISE_AND,
      Operator::BITWISE_NOT,
      Operator::SHIFTL,
      Operator::SHIFTR,
      Operator::SHIFTRR,
      Operator::PLUS,
      Operator::MINUS,
      Operator::STAR,
      Operator::MOD,
    ]

    LOGICAL_OPERATORS = [
      Operator::AND,
      Operator::OR,
      Operator::AND_NOT,
      Operator::ADJUST,
    ]

    def initialize(expression)
      @expression = expression
    end

    def build
      stack = []
      codes = @expression.codes
      codes.each do |code|
        case code.op
        when *LOGICAL_OPERATORS
          right = stack.pop
          left = stack.pop
          nodes = []
          add_logical_operation_node(code.op, nodes, left)
          add_logical_operation_node(code.op, nodes, right)
          node = ExpressionTree::LogicalOperation.new(code.op, nodes)
          stack.push(node)
        when *RELATION_OPERATORS, *ARITHMETIC_OPERATORS
          right = stack.pop
          left = stack.pop
          node = ExpressionTree::BinaryOperation.new(code.op, left, right)
          stack.push(node)
        when Operator::GET_VALUE
          node = ExpressionTree::Variable.new(code.value)
          stack.push(node)
        when Operator::PUSH
          node = ExpressionTree::Constant.new(code.value.value)
          stack.push(node)
        else
          raise "unknown operator: #{code.inspect}"
        end
      end
      stack.pop
    end

    private
    def add_logical_operation_node(operator, nodes, node)
      if node.is_a?(ExpressionTree::LogicalOperation) and
          node.operator == operator
        nodes.concat(node.nodes)
      else
        nodes << node
      end
    end
  end
end
