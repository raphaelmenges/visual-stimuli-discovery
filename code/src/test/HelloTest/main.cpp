/*

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <HelloLib/HelloLib.h>
#include <HelloIndirectLib/HelloIndirectLib.h>

// Test class declaration
class HelloTest : public ::testing::Test
{

protected:

	// Constructors
	HelloTest();
	virtual ~HelloTest();

	// Methods
	virtual void SetUp();
	virtual void TearDown();
};

// Test class definition
HelloTest::HelloTest() {}

HelloTest::~HelloTest() {};

void HelloTest::SetUp() {};

void HelloTest::TearDown() {};

// Test cases
TEST_F(HelloTest, HelloTestCase1)
{
	EXPECT_EQ(42, HelloLib::get_the_answer());
}
TEST_F(HelloTest, HelloTestCase2)
{
	EXPECT_NE(42, HelloIndirectLib::get_the_answer_indirect());
}

*/