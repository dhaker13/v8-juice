#include "ConvertDemo.hpp"
#include "v8/convert/ClassCreator.hpp"
#include "v8/convert/properties.hpp"

int BoundNative::publicStaticInt = 42;

void doFoo()
{
    CERR << "hi!\n";
}


void doNothing()
{
    CERR << "doNothing()!\n";
}

int doSomething(int x)
{
    CERR << "doSomething("<<x<<")!\n";
    return x;
}

ValueHandle sampleCallback( v8::Arguments const & argv )
{
    CERR << "sampleCallback() Arity=-1\n";
    return v8::Undefined();
}

namespace v8 { namespace convert {
    typedef NativeToJSMap<BoundNative> BMap;
    typedef NativeToJSMap<BoundSubNative> BSubMap;
    BoundNative * ClassCreator_Factory<BoundNative>::Create( v8::Persistent<v8::Object> & jsSelf, v8::Arguments const & argv )
    {
        BoundNative * b = new BoundNative;
        BMap::Insert( jsSelf, b );
        return b;
    }
    void ClassCreator_Factory<BoundNative>::Delete( BoundNative * obj )
    {
        BMap::Remove( obj );
        delete obj;
    }
    BoundSubNative * ClassCreator_Factory<BoundSubNative>::Create( v8::Persistent<v8::Object> & jsSelf, v8::Arguments const & argv )
    {
        BoundSubNative * b = new BoundSubNative;
        BSubMap::Insert( jsSelf, b );
        return b;
    }
    void ClassCreator_Factory<BoundSubNative>::Delete( BoundSubNative * obj )
    {
        BSubMap::Remove( obj );
        delete obj;
    }

#if 0 // don't use this - it doesn't work
    // An experiment.
    v8::Handle<v8::Value> ICListEnd( v8::Arguments const & )
    {
        return v8::Undefined();
    }

    // An experiment.
    template <>
    struct FunctionPtr<v8::InvocationCallback,ICListEnd>
        : SignatureBase<v8::Handle<v8::Value>, -2>
    {
        public:
        typedef FunctionSignature<v8::InvocationCallback> SignatureType;
        typedef typename SignatureType::ReturnType ReturnType;
        typedef typename SignatureType::FunctionType FunctionType;
        static FunctionType GetFunction()
        {
            return ICListEnd;
        }
        static v8::Handle<v8::Value> Call( v8::Arguments const & args  )
        {
            return ICListEnd(args);
        }
    };
    typedef FunctionPtr<v8::InvocationCallback,ICListEnd> ICForwardEOL;
    template < typename Callable0,
               typename Callable1 = ICForwardEOL,
               typename Callable2 = ICForwardEOL
               >
    struct ICForwardByArity
    {
        static v8::Handle<v8::Value> Call( v8::Arguments const & args )
        {
            int const n = args.Length();
            CERR << "ICForwardByArity::Call(): args.Length=="<<n<<'\n';
#define CHECK(N)                 CERR << "Checking against arity " << Callable##N::Arity << '\n'; \
            if( Callable##N::Arity == n ) {                             \
                return Callable##N::GetFunction()(args);                \
            } (void)0
            CHECK(0); CHECK(1); CHECK(2);
            return v8::ThrowException(StringBuffer() << "This function was passed "<<n<<" arguments, "
                                      << "but no overload was found matching that number of "
                                      << "arguments.");
        }
    };
#endif

} }


ValueHandle bogo_callback( v8::Arguments const & )
{
    return v8::Undefined();
}

int bogo_callback2( v8::Arguments const & argv )
{
    CERR << "native this=@"<< (void const *) cv::CastFromJS<BoundNative>(argv.This())
         << '\n';
    return 1;
}


ValueHandle BoundNative_toString( v8::Arguments const & argv )
{
    /*
      INTERESTING: the following JS code calls this function but we
      have a NULL 'this'.

      function test2()
      {
          var s = new BoundSubNative();
          assert(s instanceof BoundNative, "BoundSubNative is-a BoundNative");
          print('s='+s);
          s.destroy();
          // do not do this at home, kids: i'm testing weird stuff here...
          var f = new BoundNative();
          s.toString = f.toString;
          print('f='+f);
          print('s='+s); // <---- HERE
      }

      That happens because CastFromJS<BoundNative>()
      does not recognize BoundSubNative objects. Why not? To be honest, i'm
      not certain.
      
    */
    BoundNative * f = cv::CastFromJS<BoundNative>(argv.This());
    return cv::StringBuffer() << "[BoundNative Object@"<<f<<"]";
}

v8::Handle<v8::Value> bind_BoundSubNative( v8::Handle<v8::Object> dest );
char const * cstring_test( char const * c )
{
    std::cerr << "cstring_test( @"<<(void const *)c
              <<") ["<<(c ? c : "<NULL>")<<"]\n";
    return c;
}

std::string sharedString("hi, world") /* may not be static for templating reasons. */;
std::string getSharedString()
{
    CERR << "getSharedString()=="<<sharedString<<'\n';
    return sharedString;
}
void setSharedString(std::string const &s)
{
    CERR << "setSharedString("<<s<<")\n";
    sharedString = s;
}


/**
   This is an experiment...
*/
template < typename ExceptionT,
           typename SigGetMsg,
           typename v8::convert::ConstMethodSignature<ExceptionT,SigGetMsg>::FunctionType Getter,
           v8::Handle<v8::Value> (*ICB)( v8::Arguments const & )
>
v8::Handle<v8::Value> InvocationCallbackExceptionWrapper( v8::Arguments const & args )
{
    try
    {
        return ICB( args );
    }
    catch( ExceptionT const & e2 )
    {
        return v8::ThrowException(v8::convert::CastToJS((e2.*Getter)()));
    }
#if 0
    catch( std::exception const & ex )
    {
        return v8::convert::CastToJS(ex);
    }
#endif
    catch(...)
    {
        return v8::ThrowException(v8::String::New("Unknown native exception thrown!"));
    }
}

/**
   InvocationCallback wrapper which calls another InvocationCallback
   and translates native-level exceptions to JS. std::exception is caught
   explicitly and its what() method it used as the exception string. Other
   exceptions are caught and cause an unspecified exception message to be
   used.   
*/
template < v8::Handle<v8::Value> (*ICB)( v8::Arguments const & ) >
v8::Handle<v8::Value> InvocationCallbackToInvocationCallback( v8::Arguments const & args )
{
#if 1
    return InvocationCallbackExceptionWrapper<std::exception,
        char const *(),
        &std::exception::what,
        ICB>( args );
#else
    try
    {
        return ICB( args );
    }
    catch( std::exception const & ex )
    {
        return v8::convert::CastToJS(ex);
    }
    catch(...)
    {
        return v8::ThrowException(v8::String::New("Unknown native exception thrown!"));
    }
#endif
}

v8::Handle<v8::Value> test_anton_callback( v8::Arguments const & args )
{
    throw std::runtime_error("Testing Anton's callback.");
    return v8::Undefined();
}

namespace v8 { namespace convert {

    template <>
    struct ClassCreator_Init<BoundNative>
    {
        static void InitBindings( v8::Handle<v8::Object> & dest )
        {
            using namespace v8;

            ////////////////////////////////////////////////////////////
            // Bootstrap class-wrapping code...
            typedef cv::ClassCreator<BoundNative> CC;
            CC & cc( CC::Instance() );
            if( cc.IsSealed() )
            {
                cc.AddClassTo( "BoundNative", dest );
                bind_BoundSubNative(dest);
                return;
            }

            typedef cv::tmp::TypeList<
                cv::MethodToInvocable<BoundNative, void(), &BoundNative::overload0>,
                cv::MethodToInvocable<BoundNative, void(int), &BoundNative::overload1>,
                //cv::InCa< cv::MethodToInvocationCallback<BoundNative, void(int,int), &BoundNative::overload2>, 2 >
                cv::MethodToInvocable<BoundNative, void(int,int), &BoundNative::overload2>,
                cv::MethodToInvocable<BoundNative, void(v8::Arguments const &), &BoundNative::overloadN>
            > OverloadList;
            typedef cv::InCaOverloadList< OverloadList > OverloadInCas;
#if 0
            typedef FunctionPtr<InvocationCallback,ICListEnd> FPLE;
            CERR << "Arity == "<< FPLE::Arity << '\n';
            assert( -2 == FPLE::Arity );
            typedef ICForwardByArity<
            FunctionPtr< v8::InvocationCallback, MethodToInvocationCallback<BoundNative, void(), &BoundNative::overload0> >,
                FunctionPtr< v8::InvocationCallback, MethodToInvocationCallback<BoundNative, void(int), &BoundNative::overload1> >,
                FunctionPtr< v8::InvocationCallback, MethodToInvocationCallback<BoundNative, void(int,int), &BoundNative::overload2> >
                > ICOverloads;
            cc( "overloaded", ICOverloads::Call );
#endif
            
            ////////////////////////////////////////////////////////////
            // Bind some member functions...
            cc("cputs",
               cv::FunctionToInvocationCallback<int (char const *),::puts>)
                ("overloaded",
                  OverloadInCas::Call )
                ("doFoo",
                 cv::MethodToInvocationCallback<BoundNative,void (void),&BoundNative::doFoo>)
                ("doFoo2",
                 cv::MethodToInvocationCallback<BoundNative,double (int,double),&BoundNative::doFoo2>)
                ("toString",
                 cv::FunctionToInvocationCallback<ValueHandle (v8::Arguments const &),BoundNative_toString>)
                ("puts",
                 cv::ConstMethodToInvocationCallback<BoundNative,void (char const *),&BoundNative::puts>)
                ("doFooConst",
                 cv::ConstMethodToInvocationCallback<BoundNative,void (),&BoundNative::doFooConst>)
                ("invoInt",
                 cv::MethodToInvocationCallback<BoundNative, int (v8::Arguments const &), &BoundNative::invoInt>)
                ("nativeParam",
                 cv::MethodToInvocationCallback<BoundNative, void (BoundNative const *), &BoundNative::nativeParam>)
                ("nativeParamRef",
                 cv::MethodToInvocationCallback<BoundNative, void (BoundNative &), &BoundNative::nativeParamRef>)
                ("nativeParamConstRef",
                 cv::ConstMethodToInvocationCallback<BoundNative, void (BoundNative const &), &BoundNative::nativeParamConstRef>)
                ("cstr",
                 cv::FunctionToInvocationCallback< char const * (char const *), cstring_test>)
                ("destroy", CC::DestroyObjectCallback )
                ("message", "hi, world")
                ("answer", 42)
                ("anton", InvocationCallbackToInvocationCallback<test_anton_callback>)
                ("anton2", InvocationCallbackExceptionWrapper<std::exception,char const *(), &std::exception::what,
                 test_anton_callback> )
#if 1 // converting natives to JS requires more lower-level plumbing...
                 ("nativeReturn",
                 cv::MethodToInvocationCallback<BoundNative, BoundNative * (), &BoundNative::nativeReturn>)
                 ("nativeReturnConst",
                 cv::ConstMethodToInvocationCallback<BoundNative, BoundNative const * (), &BoundNative::nativeReturnConst>)
                 ("nativeReturnRef",
                 cv::MethodToInvocationCallback<BoundNative, BoundNative & (), &BoundNative::nativeReturnRef>)
                 ("nativeReturnConstRef",
                 cv::ConstMethodToInvocationCallback<BoundNative, BoundNative const & (), &BoundNative::nativeReturnConstRef>)
#endif
                ;

            ////////////////////////////////////////////////////////////////////////
            // We can of course bind them directly to the prototype, instead
            // of via the cc object:
            Handle<ObjectTemplate> const & proto( cc.Prototype() );
            proto->Set(JSTR("bogo"),
                       cv::CastToJS(cv::FunctionToInvocationCallback<ValueHandle (v8::Arguments const &), bogo_callback>)
                       );
            proto->Set(JSTR("bogo2"),
                       cv::CastToJS(cv::FunctionToInvocationCallback<int (v8::Arguments const &),bogo_callback2>)
                       );
            proto->Set(JSTR("runGC"),
                       cv::CastToJS(cv::FunctionToInvocationCallback<bool (),v8::V8::IdleNotification>)
                       );

            ////////////////////////////////////////////////////////////////////////
            // Bind some JS properties to native properties:
            typedef cv::MemberPropertyBinder<BoundNative> PB;
            PB::BindMemVar<int,&BoundNative::publicInt>( "publicIntRW", proto );
            PB::BindMemVarRO<int,&BoundNative::publicInt>( "publicIntRO", proto, true );
            PB::BindSharedVar<int,&BoundNative::publicStaticInt>("publicStaticIntRW", proto );
            PB::BindSharedVarRO<int,&BoundNative::publicStaticInt>("publicStaticIntRO", proto );
            PB::BindSharedVar<std::string,&sharedString>("staticString", proto );
            PB::BindSharedVarRO<std::string,&sharedString>("staticStringRO", proto, true );
            // More generically, accessors can be bound using this approach:
            proto->SetAccessor( JSTR("self"),
                                PB::MethodToAccessorGetter< BoundNative * (), &BoundNative::self>,
                                PB::AccessorSetterThrow );
            proto->SetAccessor( JSTR("selfRef"),
                                PB::MethodToAccessorGetter< BoundNative & (), &BoundNative::selfRef>,
                                PB::AccessorSetterThrow );
            proto->SetAccessor( JSTR("selfConst"),
                                PB::ConstMethodToAccessorGetter< BoundNative const * (), &BoundNative::self>,
                                PB::AccessorSetterThrow );
            proto->SetAccessor( JSTR("selfConstRef"),
                                PB::ConstMethodToAccessorGetter< BoundNative const & (), &BoundNative::selfRefConst>,
                                PB::AccessorSetterThrow );
            
#if 0
            PB::BindGetterFunction<std::string (), getSharedString>("sharedString2", proto);
#else
            PB::BindGetterSetterFunctions<std::string (),
                getSharedString,
                void (std::string const &),
                setSharedString>("sharedString2", proto);
#endif
            PB::BindGetterSetterMethods<int (), &BoundNative::getInt,
                void (int), &BoundNative::setInt
                >("theInt", proto);
            PB::BindNonConstGetterSetterMethods<int (), &BoundNative::getIntNonConst,
                void (int), &BoundNative::setInt
                >("theIntNC", proto);

            ////////////////////////////////////////////////////////////
            // Add class to the destination object...
            //dest->Set( JSTR("BoundNative"), cc.CtorFunction() );
            cc.AddClassTo( "BoundNative", dest );

            CERR << "Added BoundNative to JS.\n";
            if(1)
            { // sanity checking. This code should crash if the basic stuff is horribly wrong
                Handle<Value> vinst = cc.NewInstance(0,NULL);
                BoundNative * native = cv::CastFromJS<BoundNative>(vinst);
                assert( 0 != native );
                CERR << "Instantiated native BoundNative@"<<(void const *)native
                     << " via JS.\n";
                CC::DestroyObject( vinst );
            }
            bind_BoundSubNative(dest);
            CERR << "Finished binding BoundNative.\n";
        }
    };
} }

v8::Handle<v8::Value> BoundNative::bindJSClass( v8::Handle<v8::Object> dest )
{
    return cv::ClassCreator<BoundNative>::Instance().InitBindings(dest);
}

v8::Handle<v8::Value> bind_BoundSubNative( v8::Handle<v8::Object> dest )
{
    using namespace v8;
    typedef cv::ClassCreator<BoundSubNative> CC;
    CC & cc( CC::Instance() );
    if( cc.IsSealed() )
    {
        cc.AddClassTo( "BoundSubNative", dest );
        return cc.CtorFunction();
    }

    cc
        ("subFunc",
         cv::ConstMethodToInvocationCallback<BoundSubNative,void (),&BoundSubNative::subFunc>)
        ("toString",
         cv::ConstMethodToInvocationCallback<BoundSubNative,ValueHandle (),&BoundSubNative::toString>)
        ;

    typedef cv::ClassCreator<BoundNative> CCFoo;
    cc.CtorTemplate()->Inherit( CCFoo::Instance().CtorTemplate() );
    
    cc.AddClassTo("BoundSubNative",dest);
    return dest;
}
#undef JSTR


#if 0 // just an experiment.
template <typename Sig>
struct Arity;

template <typename R>
struct Arity<R ()>
{
    enum { Value = 0 };
};

template <typename R, typename A0>
struct Arity<R (A0)>
{
    enum { Value = 1 };
};
template <typename R>
struct Arity<R (v8::Arguments const &)>
{
    enum { Value = -1 };
};
template <typename Sig>
struct Arity< cv::FunctionSignature<Sig> >
{
    enum { Value = cv::FunctionSignature<Sig>::Arity };
};
template <typename T, typename Sig>
struct Arity< cv::MethodSignature<T, Sig> >
{
    enum { Value = cv::MethodSignature<T,Sig>::Arity };
};
template <typename T, typename Sig>
struct Arity< cv::ConstMethodSignature<T, Sig> >
{
    enum { Value = cv::ConstMethodSignature<T,Sig>::Arity };
};

static void arity_test()
{
    typedef Arity< int (int) > ArityCheck1;
    typedef Arity< cv::FunctionSignature<int (int)> > ArityCheck1b;
    //typedef Arity< int (int,int,int,int) > ArityCheck4;
    typedef Arity< cv::FunctionSignature<int (int,int,int,int)> > ArityCheck4b;
    //typedef Arity< int (BoundNative::*)(int) > ArityCheckT1;
    typedef Arity< cv::MethodSignature<BoundNative, int (int)> > ArityCheckT1;
    //typedef Arity< int (BoundNative::*)(int,int) > ArityCheckTC1;

#define OUT(X) CERR << #X << "::Value == " << X::Value << '\n'
    OUT(ArityCheck1);
    OUT(ArityCheck1b);
    //OUT(ArityCheck4);
    OUT(ArityCheck4b);
    OUT(ArityCheckT1);
#undef OUT
}
static const bool arity_test_bogo = (arity_test(),true);
enum {
    experimentA = Arity<char ()>::Value,
    experimentB = Arity<char (int)>::Value,
    experimentC = Arity<int (v8::Arguments const &)>::Value,
    //experimentD = Arity<int (int, int)>::Value,
    experimentZ = 0
};    
#endif // experiment
