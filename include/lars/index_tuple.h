
#pragma once

#include <tuple>
#include <stdexcept>
#include <type_traits>
#include <ostream>

#ifndef FUNCTION_REQUIRES
// Inspired by http://stackoverflow.com/questions/8743159/boostenable-if-not-in-function-signature/9220563#9220563
#define PARENTHESIS_MUST_BE_PLACED_AROUND_RETURN_TYPE(...) __VA_ARGS__>::type
#define FUNCTION_REQUIRES(REQUIREMENT) typename std::enable_if<(REQUIREMENT), PARENTHESIS_MUST_BE_PLACED_AROUND_RETURN_TYPE
#endif

namespace lars {
  
  struct DynamicIndex{
    size_t value = 0;
    constexpr DynamicIndex(){}
    constexpr DynamicIndex(size_t _value):value(_value){}
    constexpr static bool is_dynamic = true;
    void set_value(size_t v){ value = v; }
    template<size_t v> void set_value(){ value = v; }
    constexpr static size_t safe_static_value(){ return -1; }
    template <class Index> DynamicIndex & operator=(const Index &idx){ set_value(idx.value); return *this; }
    operator size_t()const{ return value; }
  };
  
  template <size_t _value> struct StaticIndex{
    constexpr StaticIndex(){ }
    StaticIndex(size_t v){ set_value(v); }
    constexpr static size_t value = _value;
    constexpr static bool is_dynamic = false;
    void set_value(size_t v){
#ifndef NDEBUG
      if(value != v) throw std::invalid_argument("setting static index to invalid value");
#endif
    }
    template<size_t v> void set_value()const{ static_assert(value == v, "setting static index to invalid value"); }
    constexpr static size_t safe_static_value(){ return _value; }
    template <size_t v> DynamicIndex & operator=(const StaticIndex<v> &idx){ set_value<idx.value>(); return *this; }
    template <class Index> DynamicIndex & operator=(const Index &idx){ set_value(idx.value); return *this; }
    operator size_t()const{ return value; }
  };
  
  template <typename ... Indices> class IndexTuple:private std::tuple<Indices...>{
  public:
    using AsTuple = std::tuple<Indices...>;
    using AsTuple::AsTuple;
    
    constexpr static size_t size(){ return sizeof...(Indices); }
    
    template <size_t i> using Element = typename std::tuple_element<i, AsTuple>::type;
    template <size_t i> constexpr static FUNCTION_REQUIRES(i != size()) (size_t) element_is_dynamic(){ return Element<i>::is_dynamic; }
    
  private:
    
    template <size_t offset,class Tuple = IndexTuple<Indices...>> struct SetIterator{
      Tuple * tuple;
      template<size_t i> void evaluate(size_t v)const{ tuple->template set_value<offset+i>(v); }
      template<size_t i,size_t v> void evaluate()const{ tuple->template set_value<offset+i,v>(); }
    };
    
    template <class F,class Tuple = IndexTuple<Indices...>> struct EvaluateIterator{
      const F * f;
      Tuple * tuple;
      template<size_t i> void evaluate(size_t v)const{ tuple->template set_value<i>(f->template evaluate<i>(v)); }
      template<size_t i,size_t v> void evaluate()const{
#ifndef NDEBUG
        tuple->template set_value<i>(f->template evaluate<i>(v));
#endif
      }
    };
    
  public:
#pragma mark Getters and Setters
    template<size_t i> size_t get()const{ return std::get<i>(*this).value; }
    template<size_t i> constexpr static size_t static_get(){ return Element<i>::value; }
    template<size_t i> void set_value(size_t v){ std::get<i>(*this).set_value(v); }
    template<size_t i,size_t v> void set_value(){ std::get<i>(*this).template set_value<v>(); }
    template<typename ... OtherIndices> void set(const IndexTuple<OtherIndices...> &other){ static_assert(size() == IndexTuple<OtherIndices...>::size(),"setting with index tuple of different size"); other.template_iterate(SetIterator<0>{this}); }
    template<size_t ... values> void set(){ set(IndexTuple<StaticIndex<values> ...>()); }
    template<size_t i> constexpr static size_t safe_static_get(){ return Element<i>::safe_static_value(); }
    template <class Other> IndexTuple<Indices ...> & operator=(const Other &other){ set(other); return *this; }
    
#pragma mark Splitting and Slicing
    template<typename ... OtherIndices> using AppendIndices = IndexTuple<Indices...,OtherIndices...>;
    template<typename ... OtherIndices> using PrependIndices = IndexTuple<OtherIndices...,Indices...>;
    template<class Other> using Append = typename Other::template PrependIndices<Indices...>;
    template<typename ... OtherIndices> AppendIndices<OtherIndices...> append(const IndexTuple<OtherIndices...> &other){
      AppendIndices<OtherIndices...> result;
      template_iterate(SetIterator<0,AppendIndices<OtherIndices...>>{&result});
      other.template_iterate(SetIterator<size(),AppendIndices<OtherIndices...>>{&result});
      return result;
    }
    
  private:
    template<size_t begin,size_t end,bool equal = begin == end> struct MakeSlice;
    template<size_t begin,size_t end> struct MakeSlice<begin,end,false>{ using type = typename MakeSlice<begin+1,end>::type::template PrependIndices<Element<begin>>; };
    template<size_t begin,size_t end> struct MakeSlice<begin,end,true>{ using type = IndexTuple<>; };
    
  public:
    template<size_t begin,size_t end> using Slice = typename MakeSlice<begin,end>::type;
    template<size_t begin,size_t end> Slice<begin,end> slice(){
      auto result = Slice<begin,end>();
      template_iterate<begin,end>(SetIterator<-begin,Slice<begin,end>>{&result});
      return result;
    }
    
#pragma mark Iteration
    template <size_t i = 0,class F = void> FUNCTION_REQUIRES(i != size()) (void) iterate(const F &f)const{ f(i,get<i>()); iterate<i+1,F>(f); }
    template <size_t i = 0,class F = void> FUNCTION_REQUIRES(i == size()) (void)  iterate(const F & f)const{  }
    
    template <size_t i = 0,size_t end = size(),class F = void> FUNCTION_REQUIRES(i != end && element_is_dynamic<i>()) (void) template_iterate(const F &f)const{ f.template evaluate<i>(get<i>()); template_iterate<i+1,end>(f); }
    template <size_t i = 0,size_t end = size(),class F = void> FUNCTION_REQUIRES(i != end && !element_is_dynamic<i>()) (void) template_iterate(const F &f)const{ f.template evaluate<i,static_get<i>()>(); template_iterate<i+1,end>(f); }
    template <size_t i = 0,size_t end = size(),class F = void> FUNCTION_REQUIRES(i==end) (void) template_iterate(const F &f)const{  }
    
#pragma mark Evaluation
  private:
    
    template <class F,size_t index,bool is_dynamic> struct MakeEvaluatedElement;
    template <class F,size_t index> struct MakeEvaluatedElement<F,index,true>{ using type = DynamicIndex; };
    template <class F,size_t index> struct MakeEvaluatedElement<F,index,false>{ using type = StaticIndex<F::template static_evaluate<index>(static_get<index>())>; };
    
    template <class F,size_t index> using EvaluatedElement = typename MakeEvaluatedElement<F,index,element_is_dynamic<index>() || !F::template can_static_evaluate<index>() >::type;
    
    template<class F,size_t index=0> struct MakeEvaluated{
      using type = typename MakeEvaluated<F,index+1>::type::template PrependIndices<EvaluatedElement<F,index>>;
    };
    
    template<class F> struct MakeEvaluated<F,size()>{
      using type = IndexTuple<>;
    };
    
    template <class F,class Other> struct BinaryEvaluator{
      const IndexTuple<Indices...> * self;
      const Other * other;
      template <size_t idx> constexpr static bool can_static_evaluate(){ return !Other::template element_is_dynamic<idx>(); }
      template <size_t idx> constexpr static size_t static_evaluate(size_t v){ return F::evaluate(static_get<idx>(),Other::template static_get<idx>()); }
      template <size_t idx> size_t evaluate(size_t v)const{ return F::evaluate(self->get<idx>(),other->template get<idx>()); }
    };
    
    struct SumEvaluator{ constexpr static size_t evaluate(size_t a,size_t b){ return a+b; } };
    struct DifferenceEvaluator{ constexpr static size_t evaluate(size_t a,size_t b){ return a-b; } };
    struct ProductEvaluator{ constexpr static size_t evaluate(size_t a,size_t b){ return a*b; } };
    struct FractionEvaluator{ constexpr static size_t evaluate(size_t a,size_t b){ return a/b; } };
    struct EqualEvaluator{ constexpr static size_t evaluate(size_t a,size_t b){ return a==b; } };
    struct UnequalEvaluator{ constexpr static size_t evaluate(size_t a,size_t b){ return a!=b; } };
    struct LessEvaluator{ constexpr static size_t evaluate(size_t a,size_t b){ return a<b; } };

  public:
    template <class F> using Evaluated =typename MakeEvaluated<F>::type;
    template <class F> Evaluated<F> evaluate(const F &f)const{
      Evaluated<F> result;
      template_iterate(EvaluateIterator<F,Evaluated<F>>{&f,&result});
      return result;
    }
    
    template <class Other> Evaluated<BinaryEvaluator<SumEvaluator,Other>> operator+(const Other &other)const{ return evaluate(BinaryEvaluator<SumEvaluator,Other>{this,&other}); }
    template <class Other> Evaluated<BinaryEvaluator<DifferenceEvaluator,Other>> operator-(const Other &other)const{ return evaluate(BinaryEvaluator<DifferenceEvaluator,Other>{this,&other}); }
    template <class Other> Evaluated<BinaryEvaluator<ProductEvaluator,Other>> operator*(const Other &other)const{ return evaluate(BinaryEvaluator<ProductEvaluator,Other>{this,&other}); }
    template <class Other> Evaluated<BinaryEvaluator<FractionEvaluator,Other>> operator/(const Other &other)const{ return evaluate(BinaryEvaluator<FractionEvaluator,Other>{this,&other}); }
    template <class Other> Evaluated<BinaryEvaluator<LessEvaluator,Other>> operator<(const Other &other)const{ return evaluate(BinaryEvaluator<LessEvaluator,Other>{this,&other}); }
    template <class Other> Evaluated<BinaryEvaluator<EqualEvaluator,Other>> element_wise_equal(const Other &other)const{ return evaluate(BinaryEvaluator<EqualEvaluator,Other>{this,&other}); }
    template <class Other> Evaluated<BinaryEvaluator<UnequalEvaluator,Other>> element_wise_unequal(const Other &other)const{ return evaluate(BinaryEvaluator<UnequalEvaluator,Other>{this,&other}); }

  private:
    template <class F,class A,class B> using ReducedResult = typename std::conditional<A::is_dynamic || B::is_dynamic, DynamicIndex, StaticIndex<F::evaluate(A::safe_static_value(),B::safe_static_value())>>::type;
    
    template <class F,class A,class B> FUNCTION_REQUIRES(A::is_dynamic || B::is_dynamic) (ReducedResult<F,A,B>) reduce(const A &a,const B &b){
      DynamicIndex res;
      res.set_value(F::evaluate(a.value,b.value));
      return res;
    }
    
    template <class F,class A,class B> FUNCTION_REQUIRES(!(A::is_dynamic || B::is_dynamic)) (ReducedResult<F,A,B>) reduce(const A &a,const B &b){ return ReducedResult<F,A,B>(); }
    
    template <class F,size_t idx> struct MakeReduced{
      using type = ReducedResult<F, typename MakeReduced<F,idx-1>::type , Element<idx-1> >;
      
      static type evaluate(IndexTuple<Indices...> * parent){ type result; result.set_value(F::evaluate(MakeReduced<F,idx-1>::evaluate(parent),parent->get<idx-1>())); return result; }
    };
    
    template <class F> struct MakeReduced<F,1>{
      using type = Element<0>;
      static type evaluate(IndexTuple<Indices...> * parent){ return std::get<0>(*parent); }
    };
    
    template <class F> struct MakeReduced<F,0>{
      using type = StaticIndex<0>;
      static type evaluate(IndexTuple<Indices...> * parent){ return type(); }
    };
    
  public:
    template <class F> using ReducedType = typename MakeReduced<F, size() >::type;
    ReducedType<SumEvaluator> sum(){ return MakeReduced<SumEvaluator, size() >::evaluate(this); }
    ReducedType<ProductEvaluator> product(){ return MakeReduced<ProductEvaluator, size() >::evaluate(this); }
    
    template <typename ... OtherIndices> bool operator==(const IndexTuple<OtherIndices...> &other)const{ return element_wise_unequal(other).sum() == 0; }
    template <typename ... OtherIndices> bool operator!=(const IndexTuple<OtherIndices...> &other)const{ return element_wise_unequal(other).sum() != 0; }
    
  };
  
  template <typename ... Indices> std::ostream &operator<<(std::ostream &stream,const IndexTuple<Indices...> index_tuple){
    stream << '(';
    index_tuple.iterate([&](size_t i,size_t v){ stream << v; if(i+1 != index_tuple.size()) stream << ','; });
    stream << ')';
    return stream;
  }
  
  struct IndexTupleUnaryIterator{
    template<size_t i> void evaluate(size_t v)const{ std::cout << "dynamic value: " << v << std::endl; }
    template<size_t i,size_t v> void evaluate()const{ std::cout << "static value: " << v << std::endl; }
  };
  
  struct SquaredEvaluator{
    template <size_t idx> constexpr static bool can_static_evaluate(){ return true; }
    template <size_t idx> constexpr static size_t static_evaluate(size_t v){ return v*v; }
    template <size_t idx> size_t evaluate(size_t v)const{ return v*v; }
  };
  
  namespace index_tuple_creators {
    template <size_t size> struct MakeDynamicIndexTuple{ using type = IndexTuple<DynamicIndex>::Append<typename MakeDynamicIndexTuple<size-1>::type>; };
    template <> struct MakeDynamicIndexTuple<0>{ using type = IndexTuple<>; };
    
    template <size_t size> struct MakeRangeIndexTuple{ using type = typename MakeRangeIndexTuple<size-1>::type::template Append<IndexTuple<StaticIndex<size-1>>>; };
    template <> struct MakeRangeIndexTuple<0>{ using type = IndexTuple<>; };
    
    template <size_t size,class I> struct MakeCopyIndexTuple{ using type = typename MakeCopyIndexTuple<size-1,I>::type::template Append<IndexTuple<I>>; };
    template <class I> struct MakeCopyIndexTuple<0,I>{ using type = IndexTuple<>; };
  }
  
  template <size_t ... indices> using StaticIndexTuple = IndexTuple<StaticIndex<indices> ... >;
  template <size_t size> using DynamicIndexTuple = typename index_tuple_creators::MakeDynamicIndexTuple<size>::type;
  template <size_t size> using RangeIndexTuple = typename index_tuple_creators::MakeRangeIndexTuple<size>::type;
  template <size_t size,class I> using CopyIndexTuple = typename index_tuple_creators::MakeCopyIndexTuple<size,I>::type;
  template <size_t v,size_t size> using IndexTupleRepeat = typename index_tuple_creators::MakeCopyIndexTuple<size,StaticIndex<v>>::type;
  
  template <typename ... Args> DynamicIndexTuple<sizeof...(Args)> make_dynamic_index_tuple(Args ... args){
    DynamicIndexTuple<sizeof...(Args)> tuple(args...);
    return tuple;
  }
  
  
}

