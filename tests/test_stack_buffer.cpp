/** StackBuffer - a thread's stack of open blocks.

ThreadStorage keeps blocks that have started but not yet finished in this buffer
(thread_storage.h). The buffer is a contiguous array of fixed capacity plus an
overflow list for unexpectedly deep nesting; once fully drained, the contiguous
part grows so that the next round avoids the overflow list.

One deliberate detail the tests pin down: destroy_elem<T> is a no-op for a
generic T (stack_buffer.h:56-60). Destructors run only for NonscopedBlock:
blocks are destroyed by hand.
*/

#include <catch2/catch_test_macros.hpp>

#include "stack_buffer.h"

#include <cstdint>
#include <vector>

namespace {

struct Payload
{
    int value;

    Payload() : value(0) {}
    explicit Payload(int v) : value(v) {}
};

} // namespace

TEST_CASE("push returns a reference to the constructed object", "[stack_buffer]")
{
    StackBuffer<Payload> buffer(8);

    Payload& first = buffer.push(42);
    REQUIRE(first.value == 42);

    Payload& second = buffer.push(7);
    REQUIRE(second.value == 7);

    REQUIRE(first.value == 42);
}

TEST_CASE("Within capacity the elements are contiguous", "[stack_buffer]")
{
    const uint32_t capacity = 8;
    StackBuffer<Payload> buffer(capacity);

    std::vector<Payload*> addresses;
    for (uint32_t i = 0; i < capacity; ++i)
        addresses.push_back(&buffer.push(static_cast<int>(i)));

    for (uint32_t i = 1; i < capacity; ++i)
    {
        const ptrdiff_t step = addresses[i] - addresses[i - 1];
        REQUIRE(step == 1);
    }

    for (uint32_t i = 0; i < capacity; ++i)
        REQUIRE(addresses[i]->value == static_cast<int>(i));
}

TEST_CASE("Exceeding capacity spills into the overflow list", "[stack_buffer]")
{
    const uint32_t capacity = 4;
    StackBuffer<Payload> buffer(capacity);

    std::vector<Payload*> inplace;
    for (uint32_t i = 0; i < capacity; ++i)
        inplace.push_back(&buffer.push(static_cast<int>(i)));

    Payload& overflowed = buffer.push(999);
    REQUIRE(overflowed.value == 999);

    const bool insideBuffer =
        (&overflowed >= inplace.front()) && (&overflowed <= inplace.back() + 1);
    REQUIRE_FALSE(insideBuffer);

    // The contiguous part neither moved nor got corrupted.
    for (uint32_t i = 0; i < capacity; ++i)
        REQUIRE(inplace[i]->value == static_cast<int>(i));
}

TEST_CASE("Deep overflow preserves every value", "[stack_buffer]")
{
    const uint32_t capacity = 4;
    StackBuffer<Payload> buffer(capacity);

    std::vector<Payload*> pushed;
    const int total = 100;
    for (int i = 0; i < total; ++i)
        pushed.push_back(&buffer.push(i));

    for (int i = 0; i < total; ++i)
        REQUIRE(pushed[static_cast<size_t>(i)]->value == i);
}

TEST_CASE("pop removes elements in LIFO order", "[stack_buffer]")
{
    StackBuffer<Payload> buffer(8);

    Payload& first = buffer.push(1);
    buffer.push(2);
    Payload& third = buffer.push(3);

    REQUIRE(third.value == 3);

    buffer.pop();
    buffer.pop();

    REQUIRE(first.value == 1);

    // The next push reuses the freed slot.
    Payload& reused = buffer.push(20);
    REQUIRE(reused.value == 20);
    REQUIRE(&reused == &first + 1);

    buffer.pop();
    buffer.pop();
}

TEST_CASE("pop drains the overflow list before the contiguous part", "[stack_buffer]")
{
    const uint32_t capacity = 4;
    StackBuffer<Payload> buffer(capacity);

    for (uint32_t i = 0; i < capacity; ++i)
        buffer.push(static_cast<int>(i));

    Payload* overflowFirst = &buffer.push(100);
    Payload* overflowSecond = &buffer.push(200);
    REQUIRE(overflowFirst != overflowSecond);

    buffer.pop();
    buffer.pop();

    // The contiguous part is still full, so this push overflows again.
    Payload& again = buffer.push(300);
    REQUIRE(again.value == 300);

    buffer.pop();
    for (uint32_t i = 0; i < capacity; ++i)
        buffer.pop();
}

TEST_CASE("The contiguous part grows once the stack is fully drained", "[stack_buffer]")
{
    // stack_buffer.h:124-130: when the stack empties and the overflow list was
    // used, the buffer is reallocated larger so the next round needs no list.
    const uint32_t capacity = 4;
    StackBuffer<Payload> buffer(capacity);

    const int deep = 10;
    for (int i = 0; i < deep; ++i)
        buffer.push(i);

    for (int i = 0; i < deep; ++i)
        buffer.pop();

    std::vector<Payload*> addresses;
    for (int i = 0; i < deep; ++i)
        addresses.push_back(&buffer.push(i));

    for (size_t i = 1; i < addresses.size(); ++i)
    {
        const ptrdiff_t step = addresses[i] - addresses[i - 1];
        REQUIRE(step == 1);
    }

    for (int i = 0; i < deep; ++i)
        buffer.pop();
}

TEST_CASE("A buffer of capacity one is usable", "[stack_buffer]")
{
    StackBuffer<Payload> buffer(1);

    Payload& only = buffer.push(5);
    REQUIRE(only.value == 5);

    Payload& overflowed = buffer.push(6);
    REQUIRE(overflowed.value == 6);
    REQUIRE(&overflowed != &only);

    buffer.pop();
    buffer.pop();

    Payload& afterDrain = buffer.push(7);
    REQUIRE(afterDrain.value == 7);
    buffer.pop();
}

TEST_CASE("Repeated fill and drain cycles", "[stack_buffer]")
{
    StackBuffer<Payload> buffer(4);

    for (int cycle = 0; cycle < 20; ++cycle)
    {
        const int depth = 3 + (cycle % 7);

        std::vector<Payload*> pushed;
        for (int i = 0; i < depth; ++i)
            pushed.push_back(&buffer.push(cycle * 100 + i));

        for (int i = 0; i < depth; ++i)
            REQUIRE(pushed[static_cast<size_t>(i)]->value == cycle * 100 + i);

        for (int i = 0; i < depth; ++i)
            buffer.pop();
    }
}
