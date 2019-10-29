#include <experimental/meta>
#include <iostream>

using namespace std::experimental;

namespace foo {
  struct S {
    int x = 10;
    float y = 40.f;
    int z = 30;
  };

  struct T {
    int y = 100;
  };
}

const char* translate_to_sql(const std::string& type_name) {
  if(type_name == "int")
    return "INTEGER";
  if(type_name == "float")
    return "FLOAT";

  return "";
  // etc.
}

template<meta::info meta_var>
void create_column_from() {
  if constexpr(!meta::is_data_member(meta_var))
    return;

  std::cout << meta::name(meta_var) << " ";
  std::cout << translate_to_sql(meta::name(meta::type(meta_var)));

  if constexpr(!meta::is_null(meta::next(meta_var))) {
    std::cout << ",";
    create_column_from<meta::next(meta_var)>();
  }
}

template<meta::info meta_class>
void create_table_from(char const*& schema_name)
{

  if constexpr(!meta::is_class(meta_class))
    return;

  std::cout << "CREATE_TABLE " << schema_name << "."
	    << meta::name(meta_class) << "(\n";

  create_column_from<meta::next(meta::front(meta_class))>();

  std::cout << ");\n";

  if constexpr(!meta::is_null(meta::next(meta_class)))
    create_table_from<meta::next(meta_class)>(schema_name);
}

template<meta::info meta_namespace>
void create_schema_from()
{
  const char* schema_name = meta::name(meta_namespace);

  std::cout << "CREATE SCHEMA " <<
  schema_name << ";\n";

  create_table_from<meta::front(meta_namespace)>(schema_name);
}

int main() {

  create_schema_from<reflexpr(foo)>();


  return 0;
}
