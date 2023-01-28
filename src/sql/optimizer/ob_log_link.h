/**
 * Copyright (c) 2021 OceanBase
 * OceanBase CE is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan PubL v2.
 * You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PubL v2 for more details.
 */

#ifndef OCEANBASE_SQL_OB_LOG_LINK_H
#define OCEANBASE_SQL_OB_LOG_LINK_H

#include "sql/optimizer/ob_logical_operator.h"

namespace oceanbase
{
namespace sql
{

typedef common::ObIArray<common::ObString> ObStringIArray;

class ObLogLink : public ObLogicalOperator
{
public:
  ObLogLink(ObLogPlan &plan);
  virtual ~ObLogLink() {}
  virtual int est_cost() override;

  virtual int generate_link_sql_post(GenLinkStmtPostContext &link_ctx) override;

  inline const common::ObIArray<ObParamPosIdx> &get_param_infos() const { return param_infos_; }
  inline const char *get_stmt_fmt_buf() const { return stmt_fmt_buf_; }
  inline int32_t get_stmt_fmt_len() const { return stmt_fmt_len_; }
  int gen_link_stmt_param_infos();
  virtual int get_plan_item_info(PlanText &plan_text,
                                ObSqlPlanItem &plan_item) override;
private:
  int print_link_stmt(char *buf, int64_t buf_len);
private:
  common::ObIAllocator &allocator_;
  char *stmt_fmt_buf_;
  int32_t stmt_fmt_len_;
  common::ObSEArray<ObParamPosIdx, 16, common::ModulePageAllocator, true> param_infos_;
};

} // namespace sql
} // namespace oceanbase

#endif // OCEANBASE_SQL_OB_LOG_LINK_H
