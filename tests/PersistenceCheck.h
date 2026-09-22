#pragma once

#include <atomic>
#include <iostream>
#include <source_location>
#include <string_view>

namespace OSF::Test
{
	inline std::atomic<int> g_failures{ 0 };

	inline bool Check(bool a_condition, std::string_view a_message,
		const std::source_location& a_location = std::source_location::current())
	{
		if (!a_condition) {
			std::cerr << a_location.file_name() << ':' << a_location.line() << ": FAIL: "
			          << a_message << '\n';
			g_failures.fetch_add(1, std::memory_order_relaxed);
		}
		return a_condition;
	}

	inline int Finish(std::string_view a_suite)
	{
		const int failures = g_failures.load(std::memory_order_relaxed);
		if (failures != 0) {
			std::cerr << failures << ' ' << a_suite << " test(s) FAILED\n";
		} else {
			std::cout << a_suite << " tests passed\n";
		}
		return failures;
	}
}

// Preserve the expression in diagnostics for suites whose assertions do not carry prose labels.
#define CHECK(a_expression) ::OSF::Test::Check(static_cast<bool>(a_expression), #a_expression)
