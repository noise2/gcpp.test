#ifndef CPPGC_TESTCASE_HPP
#define CPPGC_TESTCASE_HPP

#ifdef __unix__
#   include <stdio.h>
#   include <string.h>
#   include <unistd.h>
#   include "timer.hpp"
#endif

#undef GCPP_DEBUG
#include "../src/gcpp.hpp"
#include "../hpp/testCase.hpp"
#include "gcppStub.hpp"

#include <sstream>
#include <stdlib.h>
#include <unordered_map>

/**
 * the managed memory size
 */
#define z
#define _(a, b)     SHOULD_BE(a, b)
#define Z(x)        SHOULD_BE(gc::gc_map::get().size(), x)
#define u(a, b)     SHOULD_BE(a.use_count(), b)
#define uu(a, b)    SHOULD_BE(a.use_count(), b.use_count())
#define ptoi(p)                      reinterpret_cast<std::intptr_t>(p)
/**
 * constraint on test: in every test function's entry/exit the gc_map.size() must be equal to zero
 */
#define enter_test  cout<<"  [.]"<<__func__<<"()"; Z(0)
#define exit_test   cout<<"\r  [\u221A]"<<endl; Z(0); return true
namespace cppgc_test {
    using namespace gc;
    class gcppTestCase : public CPP_TESTER::testCase {
    public:
        void run(size_t, void**) {
            cout<<endl;
            BESURE(this->test_basic());
            BESURE(this->test_cast());
            BESURE(this->test_array());
#ifdef __unix__
            BESURE(this->test_mem());
#else
#   warning "memory test unit only supported in unix OS"
#endif
        }
    protected:
        template<typename T>
        using p = gc_ptr<T>;
        bool test_basic() {
            enter_test;
            p<int> x;
            // empty gc_ptr creation should not modify gc_map(the map)!
            Z(0);
            {
                // assigning a stack var
                x = new int(1);
                // should not modify the map
                Z(1), u(x, 1), IS_FALSE(x.stack_referred());
                // create a new instance of x, test and access it's value by gc_ptr's operator*()
                SHOULD_NOT_THROW(x = new int(*x + 1));
                // the map should be in creased by one
                Z(1);
                // the value assignment
                _(*x, 2), u(x, 1), IS_FALSE(x.stack_referred());
                // make a ref. copy
                p<int> y = &*x;
                // since we used pointing to a same point, the use count should be 2
                uu(x, y), u(x, 2), IS_FALSE(y.stack_referred());
                // just a memory copy on stack, no re assignment happened
                p<int> k = y;
                // same senario, but still the use count should be 2
                uu(x, y), uu(x, k), u(k, 2), IS_FALSE(k.stack_referred());;
                // try to change the value throw operator*()
                NOT_EQUAL(*x, 666), *x = 666, IS_EQUAL(*x, 666);
            }
            Z(1);
            IS_EQUAL(*x, 666);
            // dispose
            x.dispose();
            exit_test;
        }
        bool test_cast() {
            enter_test;
            p<derived12> surviver;
            {
                p<base1> _base1 = new base1; _base1->bval1 = 1;
                IS(*_base1, base1), _(_base1->bval1, 1);
                p<base2> _base2 = new base2; _base2->bval2 = 2;
                IS(*_base2, base2), _(_base2->bval2, 2);
                p<derived1> _d1 = new derived1; _d1->bval1 = 11, _d1->dval1 = 1;
                _(_d1->bval1, 11), _(_d1->dval1, 1);
                p<derived12> _d12 = new derived12; _d12->bval1 = 121; _d12->bval2 = 122;
                _(_d12->bval1, 121), _(_d12->bval2, 122);
                Z(4);
                _base1 = _d1;
                // last pointer dies, increments the _d1's ref count
                Z(3), _(gc_map::get().at(_d1.get_id()), 2);
                _(_base1.get_id(), _d1.get_id());
                _base1->bval1++;
                _(_base1->bval1, _d1->bval1);
                p<hderived123> _hd123 = new hderived123;
                Z(4);
                _hd123->bval3 = 1123;
                _(_hd123->bval3, 1123);
                _base2 = _hd123;
                Z(3);
                _(_base2.get_id(), _hd123.get_id());
                surviver = _hd123;
            }
            Z(1);
            p<base1> b = surviver;
            Z(1);
            b.dispose();
            Z(1);
            p<void> xp = surviver;
            surviver.dispose();
            {
                xp.dispose();
                p<void> i = new int(1);
                _(*(int*)i.get(), 1);
                i.dispose();
                _((int*)(i.get()), nullptr);
            }
            exit_test;
        }
        bool test_array() {
            enter_test;
            p<int> s = new int(0);
            {
                gc_array_ptr<int> x = {new int(-1), s.get(), new int(1)},
                y = gc_array_ptr<int>(10);
                u(s, 2);
                Z(13);
                u(y.at(1), 1);
                p<int> k = y[1];
                Z(13);
                u(k, 2);
                uu(k, y.at(1));
            }
            Z(1);
            u(s, 1);
            s.dispose();
            exit_test;
        }
#ifdef __unix__
        size_t get_mem_use(){ //Note: this value is in KB!
            FILE* file = fopen("/proc/self/status", "r");
            int result = 0;
            char line[128];
            auto parseLine = [](char* line){
                int i = strlen(line);
                while (*line < '0' || *line > '9') line++;
                line[i-3] = '\0';
                i = atoi(line);
                return i;
            };
            while (fgets(line, 128, file) != NULL){
                if (strncmp(line, "VmSize:", 7) == 0) {
                    result += parseLine(line);
                    break;
                }
            }
            fclose(file);
            return result;
        }
        bool test_mem() {
            enter_test;
            cout<<endl;
#   define  _echo(x) cout<<"     $ "<<x<<endl
            {
                typedef int test_type;
                const size_t alloc_size = 1000000;
                double sm = 0.0;
                _echo("starting allocating `"<<alloc_size<<"` usign `gc_array_ptr<>`.");
                sm = get_mem_use();
                Timer_mil t;
                gc_array_ptr<test_type> x(alloc_size);
                auto time = t.elapsed();
                auto gc_mem_use = double(get_mem_use() - sm)/1024;
                assert(gc_map::get().size() == alloc_size);
                _echo("allocation done! (in "<<time<<"ms.)");
                _echo("virtual memory use: "<<gc_mem_use<<" MB usign `gc_array_ptr<>`");
                _echo("starting deallocation `gc_array_ptr<>`.");
                t.reset();
                x.dispose();
                time = t.elapsed();
                Z(0);
                _echo("deallocation done! (in "<<time<<"ms.)");
                _echo("starting allocating `"<<alloc_size<<"` usign `::operator new()`.");
                t.reset();
                test_type** xp = new test_type*[alloc_size];
                for(size_t i = 0;i < alloc_size; i++)
                    xp[i] = new test_type;
                time = t.elapsed();
                _echo("allocation done! (in "<<time<<"ms.)");
                _echo("virtual memory use: "<<(double(sizeof(void*) + sizeof(test_type))* alloc_size) / (1024 * 1024)<<" MB usign `::operator new()`");
                _echo("starting deallocation `::operator delete()`.");
                t.reset();
                for(size_t i = 0;i < alloc_size; i++)
                    delete xp[i];
                delete[] xp;
                time = t.elapsed();
                _echo("deallocation done! (in "<<time<<"ms.)");
            }
#   undef   _echo
            exit_test;
        }
#endif
    };
}

#endif
