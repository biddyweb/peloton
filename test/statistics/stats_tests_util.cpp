//===----------------------------------------------------------------------===//
//
//                         Peloton
//
// stats_tests_util.cpp
//
// Identification: tests/include/statistics/stats_tests_util.cpp
//
// Copyright (c) 2015-16, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "statistics/stats_tests_util.h"
#include "executor/delete_executor.h"
#include "executor/executor_context.h"
#include "executor/executor_tests_util.h"
#include "executor/insert_executor.h"
#include "executor/logical_tile_factory.h"
#include "executor/mock_executor.h"
#include "executor/seq_scan_executor.h"
#include "executor/update_executor.h"
#include "expression/expression_util.h"
#include "planner/delete_plan.h"
#include "planner/insert_plan.h"
#include "planner/plan_util.h"
#include "storage/tile.h"
#include "tcop/tcop.h"

namespace peloton {
namespace test {

void StatsTestsUtil::ShowTable(std::string database_name,
                               std::string table_name) {
  catalog::Catalog::GetInstance()->GetTableWithName(database_name, table_name);
  std::unique_ptr<Statement> statement;
  auto &peloton_parser = parser::Parser::GetInstance();
  auto &traffic_cop = tcop::TrafficCop::GetInstance();

  std::vector<type::Value> params;
  std::vector<ResultType> result;
  std::string sql = "SELECT * FROM " + database_name + "." + table_name;
  statement.reset(new Statement("SELECT", sql));
  auto select_stmt = peloton_parser.BuildParseTree(sql);
  statement->SetPlanTree(
      optimizer::SimpleOptimizer().BuildPelotonPlanTree(select_stmt));
  LOG_DEBUG("%s",
            planner::PlanUtil::GetInfo(statement->GetPlanTree().get()).c_str());
  std::vector<int> result_format(statement->GetTupleDescriptor().size(), 0);
  traffic_cop.ExecuteStatementPlan(statement->GetPlanTree().get(), params,
                                   result, result_format);
}

storage::Tuple StatsTestsUtil::PopulateTuple(const catalog::Schema *schema,
                                             int first_col_val,
                                             int second_col_val,
                                             int third_col_val,
                                             int fourth_col_val) {
  auto testing_pool = TestingHarness::GetInstance().GetTestingPool();
  storage::Tuple tuple(schema, true);
  tuple.SetValue(0, type::ValueFactory::GetIntegerValue(first_col_val),
                 testing_pool);

  tuple.SetValue(1, type::ValueFactory::GetIntegerValue(second_col_val),
                 testing_pool);

  tuple.SetValue(2, type::ValueFactory::GetDecimalValue(third_col_val),
                 testing_pool);

  type::Value string_value =
      type::ValueFactory::GetVarcharValue(std::to_string(fourth_col_val));
  tuple.SetValue(3, string_value, testing_pool);
  return tuple;
}

std::shared_ptr<stats::QueryMetric::QueryParams> StatsTestsUtil::GetQueryParams(
    std::shared_ptr<uchar> &type_buf, std::shared_ptr<uchar> &format_buf,
    std::shared_ptr<uchar> &val_buf) {
  // Type
  uchar *type_buf_data = new uchar[1];
  type_buf_data[0] = 'x';
  type_buf.reset(type_buf_data);
  stats::QueryMetric::QueryParamBuf type(type_buf_data, 1);

  // Format
  uchar *format_buf_data = new uchar[1];
  format_buf_data[0] = 'y';
  format_buf.reset(format_buf_data);
  stats::QueryMetric::QueryParamBuf format(format_buf_data, 1);

  // Value
  uchar *val_buf_data = new uchar[1];
  val_buf_data[0] = 'z';
  val_buf.reset(val_buf_data);
  stats::QueryMetric::QueryParamBuf val(val_buf_data, 1);

  // Construct a query param object
  std::shared_ptr<stats::QueryMetric::QueryParams> query_params(
      new stats::QueryMetric::QueryParams(format, type, val, 1));
  return query_params;
}

void StatsTestsUtil::CreateTable(bool has_primary_key) {
  LOG_INFO("Creating a table...");

  auto id_column = catalog::Column(type::Type::INTEGER,
                                   type::Type::GetTypeSize(type::Type::INTEGER),
                                   "dept_id", true);
  if (has_primary_key) {
    catalog::Constraint constraint(CONSTRAINT_TYPE_PRIMARY, "con_primary");
    id_column.AddConstraint(constraint);
  }
  auto name_column =
      catalog::Column(type::Type::VARCHAR, 32, "dept_name", false);
  std::unique_ptr<catalog::Schema> table_schema(
      new catalog::Schema({id_column, name_column}));
  auto &txn_manager = concurrency::TransactionManagerFactory::GetInstance();
  auto txn = txn_manager.BeginTransaction();
  std::unique_ptr<executor::ExecutorContext> context(
      new executor::ExecutorContext(txn));
  planner::CreatePlan node("department_table", "emp_db",
                           std::move(table_schema),
                           CreateType::CREATE_TYPE_TABLE);
  executor::CreateExecutor create_executor(&node, context.get());
  create_executor.Init();
  create_executor.Execute();
  txn_manager.CommitTransaction(txn);
}

std::shared_ptr<Statement> StatsTestsUtil::GetInsertStmt(int id,
                                                         std::string val) {
  std::shared_ptr<Statement> statement;
  std::string sql =
      "INSERT INTO EMP_DB.department_table(dept_id,dept_name) VALUES "
      "(" +
      std::to_string(id) + ",'" + val + "');";
  LOG_TRACE("Query: %s", sql.c_str());
  statement.reset(new Statement("INSERT", sql));
  ParseAndPlan(statement.get(), sql);
  return statement;
}

std::shared_ptr<Statement> StatsTestsUtil::GetDeleteStmt() {
  std::shared_ptr<Statement> statement;
  std::string sql = "DELETE FROM EMP_DB.department_table";
  LOG_INFO("Query: %s", sql.c_str());
  statement.reset(new Statement("DELETE", sql));
  ParseAndPlan(statement.get(), sql);
  return statement;
}

std::shared_ptr<Statement> StatsTestsUtil::GetUpdateStmt() {
  std::shared_ptr<Statement> statement;
  std::string sql =
      "UPDATE EMP_DB.department_table SET dept_name = 'CS' WHERE dept_id = 1";
  LOG_INFO("Query: %s", sql.c_str());
  statement.reset(new Statement("UPDATE", sql));
  ParseAndPlan(statement.get(), sql);
  return statement;
}

void StatsTestsUtil::ParseAndPlan(Statement *statement, std::string sql) {
  auto &peloton_parser = parser::Parser::GetInstance();
  auto update_stmt = peloton_parser.BuildParseTree(sql);
  LOG_TRACE("Building plan tree...");
  statement->SetPlanTree(
      optimizer::SimpleOptimizer().BuildPelotonPlanTree(update_stmt));
  LOG_TRACE("Building plan tree completed!");
  LOG_TRACE("%s", statement->GetPlanTree().get()->GetInfo().c_str());
}

}  // namespace test
}  // namespace peloton
