#ifdef ARDUINO
#include "Arduino.h"
#else
// strlen strncmp memcpy
#include <string.h>
#include <stdint.h>
//#include <assert.h>
// cout
#include "iostream"

// http://www.geeksforgeeks.org/implement-itoa/
void strreverse(char str[], int length)
{
    int start = 0;
    int end = length -1;
    char temp;
    while (start < end)
    {
        temp = *(str+start);
        *(str+start) = *(str+end);
        *(str+end) = temp;
        //swap(*(str+start), *(str+end));
        start++;
        end--;
    }
}

// Implementation of itoa()
char* itoa(int num, char* str, int base)
{
    int i = 0;
    bool isNegative = false;

    // Handle 0 explicitely, otherwise empty string is printed for 0
    if (num == 0)
    {
        str[i++] = '0';
        str[i] = '\0';
        return str;
    }

    // In standard itoa(), negative numbers are handled only with
    // base 10. Otherwise numbers are considered unsigned.
    if (num < 0 && base == 10)
    {
        isNegative = true;
        num = -num;
    }

    // Process individual digits
    while (num != 0)
    {
        int rem = num % base;
        str[i++] = (rem > 9)? (rem-10) + 'a' : rem + '0';
        num = num/base;
    }

    // If number is negative, append '-'
    if (isNegative)
        str[i++] = '-';

    str[i] = '\0'; // Append string terminator

    // Reverse the string
    strreverse(str, i);

    return str;
}

#endif

#include "JsonVarFetch.h"

using namespace std;


/**
 * @constructor JsonVarFetch
 * Creates parser that
 * @param _arrQueries {char*} An array of strings that uniquely identifies a variable in the JSON object. Valid query would be: sensors.temperature[2].description
 * @param _nQueries {uint8_t} The number of queries provided
 * @param _strResult {char*} a char buffer that will contain the resulting value if a match was found with the processCharacter call
 * @param _nMaxLen {UINT_S} The maximum length of _strResult
 */
JsonVarFetch::JsonVarFetch( const char* _arrQueries[], uint8_t _nQueries, char* _strResult, UINT_S _nMaxLen )
{
    // Store the data locally
    this->m_arrQueries  = _arrQueries;
    this->m_nQueries    = _nQueries;
    this->m_strResult   = _strResult;
    this->m_nMaxLen     = _nMaxLen;

#ifdef DEBUG
    // Initialize the array so it looks nicer when debugging
    for ( uint8_t nState = 0; nState < ( sizeof( this->m_eParserStates ) / sizeof( ParserState ) ); nState++ )
        this->m_eParserStates[ nState ] = ParserState::Corrupt;
#endif

    this->m_nStateSize = 0;
    this->_pushState( ParserState::Uninitialized );

    // Start with zero length string
    this->m_strValue[ 0 ] = 0;

    // Iterator at 0
    this->m_nIterator = 0;

    // Build the identifier list
    this->m_arrPathMatches = new UINT_S[ QUERY_DEPTH * _nQueries ];

#ifdef DEBUG
    // Initialize all path match addresses to 'zero'
    for ( uint8_t nState = 0; nState < QUERY_DEPTH * _nQueries; nState++ )
        m_arrPathMatches[ nState ] = 0;
#endif

    // Clear the given string so we know what space is unused
    for ( UINT_S nResult = 0; nResult < _nMaxLen; nResult++ )
        _strResult[ nResult ] = 0;

    //this->m_nPathMatchLength = 1;
    uint8_t* arrPathMatchLength = new uint8_t[ _nQueries ];

    // Handle all queries
    UINT_S nChar;
    for ( uint8_t nQuery = 0; nQuery < _nQueries; nQuery++ )
    {
        nChar = 0;
        // TODO(?):Initialize the first path matches as index 'zero', which also is a separator(?)
        this->m_arrPathMatches[ QUERY_DEPTH * nQuery ] = 0;
        // Initialize the path match length(depth) as 1: a query has at least 1 identifier level
        arrPathMatchLength[ nQuery ] = 1;

        while ( nChar < UINT_MAX && _arrQueries[ nQuery ][ nChar ] )
        {
            switch ( _arrQueries[ nQuery ][ nChar ] )
            {
                // Separator (only valid ones)
                case '.':
                case '[':
                    // Start counting after the dot or index bracket
                    // Set the next level depth character offset
                    this->m_arrPathMatches[ QUERY_DEPTH * nQuery + arrPathMatchLength[ nQuery ] ] = nChar + 1;
                    // Increase current query level depth
                    arrPathMatchLength[ nQuery ]++;
                    break;
            }
            nChar++;

            // Terminate the last item with a 0 so we can determine its proper length
            // NOTE: the first path match offset is also 'zero'.
            // TODO: Probably both are used, but one of them can be inherently defined
            this->m_arrPathMatches[ QUERY_DEPTH * nQuery + arrPathMatchLength[ nQuery ] ] = 0;
        }
    }

    delete [] arrPathMatchLength;

    // Remember if the identifier we built is a full match for (one of) our quer{y|ies}
    //m_bMatchingIdentifier = false;
    m_eMatchineIdentifier = ParseStatus::Ok;
};

/**
 * @destructor
 */
JsonVarFetch::~JsonVarFetch( )
{
    delete [] this->m_arrPathMatches;
};

/**
 * Process the next character from the JSON 'stream'
 * Updates the m_eParserStates array
 * @param _c {char} next character in the JSON 'stream'
 * @returns {ParseStatus} Enum 'Ok' on success, or
 */
ParseStatus JsonVarFetch::processCharacter( char _c )
{
    UINT_S nValueLen;
    ParseStatus eParseStatus;

#if defined(DEBUG) && defined(INSANE)
    cout << _c << ": ";
#endif

    if ( m_eMatchineIdentifier == ParseStatus::CompleteFullResultDone )
    {
        return m_eMatchineIdentifier;
#if defined(DEBUG) && defined(INSANE)
        cout << "(Already done)" << endl;
#endif
    }

    switch ( this->_peekState( ) )
    {
        case ParserState::Uninitialized:
            // New JSON object, allow 'whitespace' but expect '{'
            if ( _isWhiteSpace( _c ) )
            {
#if defined(DEBUG) && defined(INSANE)
                cout << "Ok (object START whitespace)" << endl;
#endif
                return ParseStatus::Ok;
            }

            if ( _c != '{' )
            {
#if defined(DEBUG) && defined(INSANE)
                cout << "JsonError (object START)" << endl;
#endif
                return ParseStatus::JsonError;
            }

            // JSON starts as an object
            this->_pushState( ParserState::Object );
#if defined(DEBUG) && defined(INSANE)
            cout << "Ok (object START)" << endl;
#endif
            return ParseStatus::Ok;

        case ParserState::Object:
            // Object, allow 'whitespace' but expect '\"'
            if ( _isWhiteSpace( _c ) )
            {
#if defined(DEBUG) && defined(INSANE)
                cout << "Ok (object identifier whitespace)" << endl;
#endif
                return ParseStatus::Ok;
            }

            if ( _c != '\"' )
            {
#if defined(DEBUG) && defined(INSANE)
                cout << "JsonError (object identifier)" << endl;
#endif
                return ParseStatus::JsonError;
            }

            // Start of pair
            this->_pushState( ParserState::Pair );
#if defined(DEBUG) && defined(INSANE)
            cout << "Ok (object identifier)" << endl;
#endif
            return ParseStatus::Ok;

        case ParserState::Array:
            // Should not reach this (Array is always immediately followed by Value on the stack
#if defined(DEBUG) && defined(INSANE)
            cout << "ParserError (array)" << endl;
#endif
            return ParseStatus::ParserError;

        case ParserState::Value:
            // Value, allow 'whitespace'
            if ( _isWhiteSpace( _c ) )
            {
#if defined(DEBUG) && defined(INSANE)
                cout << "Ok (whitespace value)" << endl;
#endif
                return ParseStatus::Ok;
            }

            // Determine what incoming character we have
            switch ( _c )
            {
                case '{':
                    this->_swapState( ParserState::Object );
#if defined(DEBUG) && defined(INSANE)
                    cout << "Ok (object push value)" << endl;
#endif
                    return ParseStatus::Ok;

                case '[':
                    // Pop the Value state
                    this->_popState( );

                    // Push the current iterator
                    // NOTE: this is somewhat ugly
                    this->_pushState( (ParserState)this->m_nIterator );

                    // Push array type on the stack, followed by Value
                    this->_pushState( ParserState::Array );
                    this->_pushState( ParserState::Value );

                    // Reset the iterator
                    this->m_nIterator = 0;
                    this->_applyIteratorIdentifier();
#ifdef DEBUG
                    this->_printDebugData( ParserState::Identifier );
#endif
                    // Match identifier 0 with the current identifiers
                    m_eMatchineIdentifier = this->_checkCompleteIdentifier();
                    this->m_strValue[ 0 ] = 0;

#if defined(DEBUG) && defined(INSANE)
                    printParseStatus( m_eMatchineIdentifier );
                    cout << " (array push value)" << endl;
#endif
                    return m_eMatchineIdentifier;
                    //return ParseStatus::Ok;

                case '\"':
#ifdef DEBUG
                    this->_printDebugData( ParserState::Uninitialized );
#endif
                    this->_swapState( ParserState::String );
#if defined(DEBUG) && defined(INSANE)
                    cout << "Ok (string value)" << endl;
#endif
                    return ParseStatus::Ok;

                case 't': // true
                case 'f': // false
                case 'n': // null
                    // This is the boolean/special primitive: true/false/null is allowed
                    // Add special value to the identifier string
                    // TODO: only if complete and matching identifier
#ifdef DEBUG
                    //dummy();
#endif
                    if ( m_eMatchineIdentifier >= ParseStatus::CompletePartialResult && !this->_addCharacterToIdentifier( _c ) )
                    {
#ifdef DEBUG
                        print( (char*)"E: Value, primitive" );
#endif
#if defined(DEBUG) && defined(INSANE)
                        cout << "AllocationError (primitive value)" << endl;
#endif
                        return ParseStatus::AllocationError;
                    }

                    this->_swapState( ParserState::Special );
#if defined(DEBUG) && defined(INSANE)
                    printParseStatus( m_eMatchineIdentifier );
                    //cout << "Ok";
                    cout << " (primitive value)" << endl;
#endif
                    return ParseStatus::Ok;

                case '-':
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                    // Add value to the identifier string
                    // TODO: only if complete and matching identifier
                    if ( m_eMatchineIdentifier >= ParseStatus::CompletePartialResult && !this->_addCharacterToIdentifier( _c ) )
                    {
#ifdef DEBUG
                        print( (char*)"E: Value, number" );
#endif
#if defined(DEBUG) && defined(INSANE)
                        cout << "AllocationError (digit value)" << endl;
#endif
                        return ParseStatus::AllocationError;
                    }

                    // Switch to number mode
                    this->_swapState( ParserState::Number );
#if defined(DEBUG) && defined(INSANE)
                    cout << "Ok (digit value)" << endl;
#endif
                    return ParseStatus::Ok;

                default:
#if defined(DEBUG) && defined(INSANE)
                    cout << "ParserError (digit value)" << endl;
#endif
                    return ParseStatus::ParserError;
            }

        case ParserState::String:
            if ( _c == '\"' )
            {
#if defined(DEBUG) && defined(DETAIL)
                this->_printDebugData( this->_peekState( ) );
#endif
                // If complete
                eParseStatus = this->_checkCompleteValue( );
                this->m_strValue[ 0 ] = 0;
                if ( eParseStatus < ParseStatus::Ok )
                {
#if defined(DEBUG) && defined(INSANE)
                    cout << "Not Ok (string)" << endl;
#endif
                    return eParseStatus;
                }

                // String complete
                this->_swapState( ParserState::NextSegment );
#if defined(DEBUG) && defined(INSANE)
                cout << "Ok (string)" << endl;
#endif
                return ParseStatus::Ok;
            }

            // Add the string character to the identifier
            // TODO: only if complete and matching identifier
            if ( m_eMatchineIdentifier >= ParseStatus::CompletePartialResult && !this->_addCharacterToIdentifier( _c ) )
            {
#ifdef DEBUG
                print( (char*)"E: String" );
#endif
#if defined(DEBUG) && defined(INSANE)
                cout << "AllocationError (string)" << endl;
#endif
                return ParseStatus::AllocationError;
            }

            if ( m_eMatchineIdentifier == ParseStatus::CompleteFullResult )
                m_eMatchineIdentifier = ParseStatus::CompleteFullResultDone;

#if defined(DEBUG) && defined(INSANE)
            printParseStatus( m_eMatchineIdentifier );
            cout << " (string)" << endl;
#endif
            return m_eMatchineIdentifier;
            //return ParseStatus::Ok;

        case ParserState::Pair:
            if ( _c == '\"' )
            {
#ifdef DEBUG
                this->_printDebugData( ParserState::Pair );
#endif

                // Match complated string identifier with the current identifiers
                m_eMatchineIdentifier = this->_checkCompleteIdentifier();
                this->m_strValue[ 0 ] = 0;

                // Pair identifier complete
                this->_swapState( ParserState::Identifier );
#if defined(DEBUG) && defined(INSANE)
                printParseStatus( m_eMatchineIdentifier );
                cout << " (pair identifier complete)" << endl;
#endif
                return m_eMatchineIdentifier;
                //return ParseStatus::Ok;
            }

            // Add the character to the identifier string
#ifdef ALLOW_TRUNCATED_IDENTIFIERS
            this->_addCharacterToIdentifier( _c );
#else
            if ( !this->_addCharacterToIdentifier( _c ) )
            {
#ifdef DEBUG
                print( (char*)"E: Identifier" );
#endif
#if defined(DEBUG) && defined(INSANE)
                cout << "AllocationError (pair identifier)" << endl;
#endif
                return ParseStatus::AllocationError;
            }
#endif
#if defined(DEBUG) && defined(INSANE)
            cout << "Ok (pair identifier)" << endl;
#endif
            return ParseStatus::Ok;

        case ParserState::Identifier:
            // Identifier waiting on colon, allow 'whitespace'
            if ( _isWhiteSpace( _c ) )
            {
#if defined(DEBUG) && defined(INSANE)
                cout << "Ok (identifier whitespace)" << endl;
#endif
                return ParseStatus::Ok;
            }

            // No colon; this should not happen
            if ( _c != ':' )
            {
#if defined(DEBUG) && defined(INSANE)
                cout << "JsonError (identifier colon)" << endl;
#endif
                return ParseStatus::JsonError;
            }

            this->_swapState( ParserState::Value );
#if defined(DEBUG) && defined(INSANE)
            cout << "Ok (identifier colon)" << endl;
#endif
            return ParseStatus::Ok;

        case ParserState::Number:
            // (-?)(0|[1-9][0-9]+)(\.[0-9]+)?
            // TODO

            /*
            // Whitespace finishes value, ready for next segment
            if ( _isWhiteSpace( _c ) )
            {
                this->_swapState( ParserState::NextSegment );
                return ParseStatus::Ok;
            }
            */

            switch ( _c )
            {
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                case '.':
                    // TODO: only if complete and matching identifier
                    if ( m_eMatchineIdentifier >= ParseStatus::CompletePartialResult && !this->_addCharacterToIdentifier( _c ) )
                    {
#ifdef DEBUG
                        print( (char*)"E: Number" );
#endif
#if defined(DEBUG) && defined(INSANE)
                        cout << "AllocationError (digit number)" << endl;
#endif
                        return ParseStatus::AllocationError;
                    }

#if defined(DEBUG) && defined(INSANE)
                    printParseStatus( m_eMatchineIdentifier );
                    cout << " (digit number)" << endl;
#endif
                    return m_eMatchineIdentifier;
                    //return ParseStatus::Ok;

                case ',':
                    // Immediate next value/pair
#if defined(DEBUG) && defined(DETAIL)
                    this->_printDebugData( this->_peekState( ) );
#endif
                    // If complete
                    eParseStatus = this->_checkCompleteValue( );
                    this->m_strValue[ 0 ] = 0;
                    if ( eParseStatus < ParseStatus::Ok )
                    {
#if defined(DEBUG) && defined(INSANE)
                        cout << "Not Ok (next segment number)" << endl;
#endif
                        return eParseStatus;
                    }

                    this->_popState( );
                    switch ( this->_peekState( ) )
                    {
                        case ParserState::Object:
                            // If we are iterating an Object, do nothing so we can accept Pair mode
#if defined(DEBUG) && defined(INSANE)
                            cout << "Ok (object next segment number)" << endl;
#endif
                            return ParseStatus::Ok;

                        case ParserState::Array:
                            // If we are iterating an Array, increase the iterator and switch to Value mode
                            // Increase the iterator in case of Array
                            this->m_nIterator++;
                            this->_applyIteratorIdentifier();
#ifdef DEBUG
                            this->_printDebugData( ParserState::Identifier );
#endif
                            // Match identifier m_nIterator with the current identifiers (previous value was a number)
                            m_eMatchineIdentifier = this->_checkCompleteIdentifier();
                            this->m_strValue[ 0 ] = 0;

                            this->_pushState( ParserState::Value );

                            if ( m_eMatchineIdentifier == ParseStatus::CompleteFullResult )
                                m_eMatchineIdentifier = ParseStatus::CompleteFullResultDone;

#if defined(DEBUG) && defined(INSANE)
                            printParseStatus( m_eMatchineIdentifier );
                            cout << " (array next segment number)" << endl;
#endif
                            return m_eMatchineIdentifier;
                            //return ParseStatus::Ok;

                        default:
#if defined(DEBUG) && defined(INSANE)
                            cout << "JsonError (next segment number)" << endl;
#endif
                            return ParseStatus::JsonError;
                    }

                case '}':
                    // Tear down the stack (object)
#if defined(DEBUG) && defined(DETAIL)
                    this->_printDebugData( this->_peekState( ) );
#endif
                    // If complete
                    eParseStatus = this->_checkCompleteValue( );
                    this->m_strValue[ 0 ] = 0;
                    if ( eParseStatus < ParseStatus::Ok )
                    {
#if defined(DEBUG) && defined(INSANE)
                        cout << "Not Ok (object pop number)" << endl;
#endif
                        return eParseStatus;
                    }

                    // Pop Number
                    this->_popState( );

                    // Pop Object
                    switch ( this->_popState( ) )
                    {
                        case ParserState::Object:
                            // End of array, ready for a new segment (on its parent)
                            this->_pushState( ParserState::NextSegment );
#if defined(DEBUG) && defined(INSANE)
                            cout << "Ok (object pop number)" << endl;
#endif
                            return ParseStatus::Ok;

                        default:
#if defined(DEBUG) && defined(INSANE)
                            cout << "JsonError (object pop number)" << endl;
#endif
                            return ParseStatus::JsonError;
                    }

                case ']':
                    // Tear down the stack (array)
#if defined(DEBUG) && defined(DETAIL)
                    this->_printDebugData( this->_peekState( ) );
#endif
                    // If complete
                    eParseStatus = this->_checkCompleteValue( );
                    this->m_strValue[ 0 ] = 0;
                    if ( eParseStatus < ParseStatus::Ok )
                    {
#if defined(DEBUG) && defined(INSANE)
                        cout << "Not Ok (array pop number)" << endl;
#endif
                        return eParseStatus;
                    }

                    // Pop Number
                    this->_popState( );

                    // Pop Array
                    switch ( this->_popState( ) )
                    {
                        case ParserState::Array:
                            // End of array, ready for a new segment (on its parent)
                            // Pop the previous iterator
                            // NOTE: this is somewhat ugly
                            this->m_nIterator = (uint8_t)this->_popState( );

                            this->_pushState( ParserState::NextSegment );
#if defined(DEBUG) && defined(INSANE)
                            cout << "Ok (array pop number)" << endl;
#endif
                            return ParseStatus::Ok;

                        default:
#if defined(DEBUG) && defined(INSANE)
                            cout << "JsonError (array pop number)" << endl;
#endif
                            return ParseStatus::JsonError;
                    }

                case ' ':
                case '\t':
                case '\n':
                case '\r':
                    // Whitespace finishes value, ready for next segment
#if defined(DEBUG) && defined(DETAIL)
                    this->_printDebugData( this->_peekState( ) );
#endif
                    // If complete
                    eParseStatus = this->_checkCompleteValue( );
                    this->m_strValue[ 0 ] = 0;
                    if ( eParseStatus < ParseStatus::Ok )
                    {
#if defined(DEBUG) && defined(INSANE)
                        cout << "Not Ok (whitespace number)" << endl;
#endif
                        return eParseStatus;
                    }

                    this->_swapState( ParserState::NextSegment );
#if defined(DEBUG) && defined(INSANE)
                    cout << "Ok (whitespace number)" << endl;
#endif
                    return ParseStatus::Ok;

                default:
#if defined(DEBUG) && defined(INSANE)
                    cout << "JsonError (whitespace number)" << endl;
#endif
                    return ParseStatus::JsonError;
            }

        case ParserState::Special:
            // This is the boolean/special primitive: true/false/null is allowed
            // TODO

            /*
            // Whitespace finishes value, ready for next segment
            if ( _isWhiteSpace( _c ) )
            {
                this->_swapState( ParserState::NextSegment );
                return ParseStatus::Ok;
            }
            */

            switch ( _c )
            {
                case 'r':
                case 'u':
                case 'e':
                case 'a':
                case 'l':
                case 's':
                    // Add special value to the identifier string
                    // TODO: only if complete and matching identifier
                    if ( m_eMatchineIdentifier >= ParseStatus::CompletePartialResult && !this->_addCharacterToIdentifier( _c ) )
                    {
#ifdef DEBUG
                        print( (char*)"E: Special, primitive" );
#endif
#if defined(DEBUG) && defined(INSANE)
                        cout << "Allocation Error (primitive special)" << endl;
#endif
                        return ParseStatus::AllocationError;
                    }

#if defined(DEBUG) && defined(INSANE)
                    printParseStatus( m_eMatchineIdentifier );
                    //cout << "Ok";
                    cout << " (primitive special)" << endl;
#endif
                    //return m_eMatchineIdentifier;
                    return ParseStatus::Ok;

                case ',':
                    // Immediate next value/pair
#if defined(DEBUG) && defined(DETAIL)
                    this->_printDebugData( this->_peekState( ) );
#endif
                    // If complete
                    eParseStatus = this->_checkCompleteValue( );
                    this->m_strValue[ 0 ] = 0;
                    if ( eParseStatus < ParseStatus::Ok )
                    {
#if defined(DEBUG) && defined(INSANE)
                        cout << "Not Ok (next segment special)" << endl;
#endif
                        return eParseStatus;
                    }

#if defined(DEBUG) && defined(INSANE)
/*
            cout << "QUERY 1 DEBUG ";
            for ( uint8_t nMatch = 0; nMatch < QUERY_DEPTH; nMatch++ )
            {
                cout << unsigned(this->m_arrPathMatches[ QUERY_DEPTH * 1 + nMatch ]) << " ";
            }
*/
#endif

                    this->_popState( );
                    switch ( this->_peekState( ) )
                    {
                        case ParserState::Object:
                            // If we are iterating an Object, do nothing so we can accept Pair mode
                            //m_eMatchineIdentifier = this->_checkCompleteIdentifier();

#if defined(DEBUG) && defined(INSANE)
                            printParseStatus( m_eMatchineIdentifier ); //m_eMatchineIdentifier  eParseStatus
                            cout << " (object next segment special)" << endl;
#endif
                            return m_eMatchineIdentifier;//ParseStatus::Ok;

                        case ParserState::Array:
                            // If we are iterating an Array, increase the iterator and switch to Value mode
                            // Increase the iterator in case of Array
                            this->m_nIterator++;
                            this->_applyIteratorIdentifier();
#ifdef DEBUG
                            this->_printDebugData( ParserState::Identifier );
#endif
                            // Match identifier m_nIterator with the current identifiers (previous value was a boolean primitive)
                            m_eMatchineIdentifier = this->_checkCompleteIdentifier();
                            this->m_strValue[ 0 ] = 0;

                            this->_pushState( ParserState::Value );
#if defined(DEBUG) && defined(INSANE)
                            printParseStatus( m_eMatchineIdentifier );
                            cout << " (array next segment special)" << endl;
#endif
                            return m_eMatchineIdentifier;
                            //return ParseStatus::Ok;

                        default:
#if defined(DEBUG) && defined(INSANE)
                            cout << "JsonError (next segment special)" << endl;
#endif
                            return ParseStatus::JsonError;
                    }

                case '}':
                    // Tear down the stack (object)
#if defined(DEBUG) && defined(DETAIL)
                    this->_printDebugData( this->_peekState( ) );
#endif
                    // If complete
                    eParseStatus = this->_checkCompleteValue( );
                    this->m_strValue[ 0 ] = 0;
                    if ( eParseStatus < ParseStatus::Ok )
                    {
#if defined(DEBUG) && defined(INSANE)
                        cout << "Not Ok (object pop special)" << endl;
#endif
                        return eParseStatus;
                    }

                    // Pop Number
                    this->_popState( );

                    // Pop Object
                    switch ( this->_popState( ) )
                    {
                        case ParserState::Object:
                            // End of array, ready for a new segment (on its parent)
                            this->_pushState( ParserState::NextSegment );
#if defined(DEBUG) && defined(INSANE)
                            cout << "Ok (object pop special)" << endl;
#endif
                            return ParseStatus::Ok;

                        default:
#if defined(DEBUG) && defined(INSANE)
                            cout << "JsonError (object pop special)" << endl;
#endif
                            return ParseStatus::JsonError;
                    }

                case ']':
                    // Tear down the stack (array)
#if defined(DEBUG) && defined(DETAIL)
                    this->_printDebugData( this->_peekState( ) );
#endif
                    // If complete
                    eParseStatus = this->_checkCompleteValue( );
                    this->m_strValue[ 0 ] = 0;
                    if ( eParseStatus < ParseStatus::Ok )
                    {
#if defined(DEBUG) && defined(INSANE)
                        cout << "Not Ok (array pop special)" << endl;
#endif
                        return eParseStatus;
                    }

                    // Pop Number
                    this->_popState( );

                    // Pop Array
                    switch ( this->_popState( ) )
                    {
                        case ParserState::Array:
                            // Pop the previous iterator
                            // NOTE: this is somewhat ugly
                            this->m_nIterator = (uint8_t)this->_popState( );

                            // End of array, ready for a new segment (on its parent)
                            this->_pushState( ParserState::NextSegment );
#if defined(DEBUG) && defined(INSANE)
                            cout << "Ok (array pop special)" << endl;
#endif
                            return ParseStatus::Ok;

                        default:
#if defined(DEBUG) && defined(INSANE)
                            cout << "JsonError (array special)" << endl;
#endif
                            return ParseStatus::JsonError;
                    }

                case ' ':
                case '\t': // tab
                case '\n': // newline
                case '\r': // return
                    // Whitespace finishes value, ready for next segment
#if defined(DEBUG) && defined(DETAIL)
                    this->_printDebugData( this->_peekState( ) );
#endif
                    // If complete
                    eParseStatus = this->_checkCompleteValue( );
                    this->m_strValue[ 0 ] = 0;
                    if ( eParseStatus < ParseStatus::Ok )
                    {
#if defined(DEBUG) && defined(INSANE)
                        cout << "Not Ok (complete value whitespace special)" << endl;
#endif
                        return eParseStatus;
                    }

                    this->_swapState( ParserState::NextSegment );
#if defined(DEBUG) && defined(INSANE)
                cout << "Ok (whitespace special)" << endl;
#endif
                    return ParseStatus::Ok;

                default:
#if defined(DEBUG) && defined(INSANE)
                    cout << "JsonError (unhandled special)" << endl;
#endif
                    return ParseStatus::JsonError;
            }

        case ParserState::NextSegment:
            if ( _isWhiteSpace( _c ) )
            {
#if defined(DEBUG) && defined(INSANE)
                cout << "Ok (whitespace)" << endl;
#endif
                return ParseStatus::Ok;
            }

            // Determine what incoming character we have
            switch ( _c )
            {
                case '}':
                    // Tear down the stack (object)
                    // Pop NextSegment
                    this->_popState( );

                    // Pop Object
                    switch ( this->_popState( ) )
                    {
                        case ParserState::Object:
                            // End of array, ready for a new segment (on its parent)
                            this->_pushState( ParserState::NextSegment );
#if defined(DEBUG) && defined(INSANE)
                            cout << "Ok (object pop)" << endl;
#endif
                            return ParseStatus::Ok;

                        default:
#if defined(DEBUG) && defined(INSANE)
                            cout << "JsonError (not an object)" << endl;
#endif
                            return ParseStatus::JsonError;
                    }

                case ']':
                    // Tear down the stack (array)
                    // Pop NextSegment
                    this->_popState( );

                    // Pop Array
                    switch ( this->_popState( ) )
                    {
                        case ParserState::Array:
                            // Pop the previous iterator
                            // NOTE: this is somewhat ugly
                            this->m_nIterator = (uint8_t)this->_popState( );

                            // End of array, ready for a new segment (on its parent)
                            this->_pushState( ParserState::NextSegment );
#if defined(DEBUG) && defined(INSANE)
                            cout << "Ok (array pop)" << endl;
#endif
                            return ParseStatus::Ok;

                        default:
#if defined(DEBUG) && defined(INSANE)
                            cout << "JsonError (not an array)" << endl;
#endif
                            return ParseStatus::JsonError;
                    }

                case ',':
                    // Next value/pair
                    this->_popState( );
                    switch ( this->_peekState( ) )
                    {
                        case ParserState::Object:
                            // If we are iterating an Object, do nothing so we can accept Pair mode
#if defined(DEBUG) && defined(INSANE)
                            cout << "Ok (object next segment)" << endl;
#endif
                            return ParseStatus::Ok;

                        case ParserState::Array:
                            // If we are iterating an Array, increase the iterator and switch to Value mode
                            // Increase the iterator in case of Array
                            this->m_nIterator++;
                            this->_applyIteratorIdentifier();
#ifdef DEBUG
                            this->_printDebugData( ParserState::Identifier );
#endif
                            // Match identifier m_nIterator with the current identifiers (new value pair)
                            m_eMatchineIdentifier = this->_checkCompleteIdentifier();
                            this->m_strValue[ 0 ] = 0;

                            this->_pushState( ParserState::Value );

#if defined(DEBUG) && defined(INSANE)
                            printParseStatus( m_eMatchineIdentifier );
                            cout << " (array next segment)" << endl;
#endif
                            return m_eMatchineIdentifier;
                            //return ParseStatus::Ok;

                        default:
#if defined(DEBUG) && defined(INSANE)
                            cout << "JsonError (next segment without object or array)" << endl;
#endif
                            return ParseStatus::JsonError;
                    }

                default:
#if defined(DEBUG) && defined(INSANE)
                    cout << "ParserError (unknown char in NextSegment)" << endl;
#endif
                    return ParseStatus::ParserError;
            }

#if defined(DEBUG) && defined(INSANE)
            cout << "Ok ()" << endl;
#endif
            return ParseStatus::Ok;

        default:
#if defined(DEBUG) && defined(INSANE)
            cout << "ParserError (unknown parser state)" << endl;
#endif
            return ParseStatus::ParserError;
    }
};

/**
 * push new parser state onto the stack
 * @param _eParserState {ParserState} The new state
 * @returns {Boolean} true if succeeded, false on stack overflow
 */
bool JsonVarFetch::_pushState( ParserState _eParserState )
{
    // Check for stack overflow
    if ( this->m_nStateSize >= ( sizeof( this->m_eParserStates ) / sizeof( ParserState ) ) )
        return false;

    // Push element success
    this->m_eParserStates[ this->m_nStateSize++ ] = _eParserState;
    return true;
};

/**
 * pop current parser state from the stack, removing it
 * @returns {ParserState} the parser state that was current (now removed), or Corrupt if the stack is empty
 */
ParserState JsonVarFetch::_popState( )
{
    // Stack corruption?
    if ( this->m_nStateSize == 0 )
        return ParserState::Corrupt;

#ifdef DEBUG
    ParserState eOldState = this->m_eParserStates[ --this->m_nStateSize ];
    this->m_eParserStates[ this->m_nStateSize ] = ParserState::Corrupt;
    return eOldState;
#endif

    return this->m_eParserStates[ --this->m_nStateSize ];
};

/**
 * peek for parser state on the stack without modifying it
 * @returns {ParserState} the current parser state, or Corrupt if the stack is empty
 */
ParserState JsonVarFetch::_peekState( )
{
    // Stack corruption
    if ( this->m_nStateSize == 0 )
        return ParserState::Corrupt;

    return this->m_eParserStates[ this->m_nStateSize - 1 ];
};

/**
 * peek for parser state on the stack without modifying it
 * @param _bListType {Boolean} continue iterating the stack until a list type is found (Object/Array)
 * @returns {ParserState} the 'found' parser state, or Corrupt if the stack is empty/list type state not found
 */
ParserState JsonVarFetch::_peekState( bool _bListType )
{
    if ( this->m_nStateSize == 0 )
        return ParserState::Corrupt;

    // Give back the parser state enum, or bubble down up to a list type element
    if ( _bListType )
    {
        for ( uint8_t nState = this->m_nStateSize - 1; nState > 0; nState-- )
        {
            switch ( this->m_eParserStates[ nState ] )
            {
                case ParserState::Object:
                    return ParserState::Object;
                case ParserState::Array:
                    return ParserState::Array;
            }
        }

        // Stack corruption?
        return ParserState::Corrupt;
    } else {
        return this->m_eParserStates[ this->m_nStateSize - 1 ];
    }
};

/**
 * pop current parser state from the stack, push new parser state onto the stack
 * @param _eParserState {ParserState} The new state
 * @returns {ParserState} the previous parser state, or Corrupt if the stack is empty
 */
ParserState JsonVarFetch::_swapState( ParserState _eParserState )
{
    ParserState eOldState;
    // Stack corruption?
    if ( this->m_nStateSize == 0 )
        return ParserState::Corrupt;

    eOldState = this->m_eParserStates[ this->m_nStateSize - 1 ];
    this->m_eParserStates[ this->m_nStateSize - 1 ] = _eParserState;

    return eOldState;
};

/**
 * Get the current parser depth by counting the number of list type states on the stack
 * @returns {uint8_t} The depth (level) in the current parser state
 */
uint8_t JsonVarFetch::_getLevel( )
{
    uint8_t nLevel = 0;

    for ( uint8_t nState = this->m_nStateSize - 1; nState > 0; nState-- )
    {
        switch ( this->m_eParserStates[ nState ] )
        {
            case ParserState::Object:
            case ParserState::Array:
                nLevel++;
        }
    }

    return nLevel;
};

/**
 * Determine if the given char is a whitespace character
 * @param _c {char} The char to be tested
 * @returns {Boolean} true if space, tab, return or newline, false otherwise
 */
inline bool JsonVarFetch::_isWhiteSpace( char _c )
{
    switch ( _c )
    {
        case ' ':
        case '\t':
        case '\n':
        case '\r':
            return true;

        default:
            return false;
    }
};

/**
 * Add character to the identifier (either key or value; iterater done in _applyIteratorIdentifier)
 * @param _c {char} The character to add to the identifier
 * @returns {Boolean} true if succeeded, false if there was no space left
 */
bool JsonVarFetch::_addCharacterToIdentifier( char _c )
{
    UINT_S nValueLen = strlen( this->m_strValue );

    // Check if we have room for this character (currently: VARIABLE_LENGTH)
    if ( nValueLen >= sizeof( this->m_strValue ) )
        return false;

/*
#if defined(DEBUG) && defined(INSANE)
    // note: inline
    cout << "adding [" << _c << "] at index [" << nValueLen << "]";
#endif
*/

    // Add character and string termination
    this->m_strValue[ nValueLen++ ] = _c;
    this->m_strValue[ nValueLen ] = 0;
    return true;
};

/**
 * Apply the current iterator as a string identifier (iterator; key and value done in _addCharacterToIdentifier)
 * @returns {ParseStatus} Ok if succeeded, AllocationError if there was no space to fit the current iterator value
 */
ParseStatus JsonVarFetch::_applyIteratorIdentifier( )
{
    // TODO: Cheesy (invalid) solution (currently: VARIABLE_LENGTH would mean pow( 10, 5 ) - 1 and -pow( 10, 5 - 1 ) + 1 )
    if ( this->m_nIterator > 99999999999999 || this->m_nIterator < -9999999999999 )
        return ParseStatus::AllocationError;

    // Do the actual conversion
    itoa( this->m_nIterator, this->m_strValue, 10 );

    return ParseStatus::Ok;
};

/**
 * Checks whether the completed identifier matches any of the given queries (not matched previously)
 * sets m_bMatchingIdentifier if there are 1 or more queries that matched
 * @returns {ParseStatus} Complete, CompletePartialResult, CompleteFullResult
 */
ParseStatus JsonVarFetch::_checkCompleteIdentifier( )
{
    UINT_S nMatchStartPos;
    uint8_t nCurrentLevel = this->_getLevel( ) - 1;
    UINT_S nValueLength = strlen( this->m_strValue );
    ParseStatus status = ParseStatus::Ok;
    uint8_t nMatchCount = 0;

    bool bDebug = false;
#ifdef DEBUG
    if ( !strncmp( this->m_strValue, "open", 4 ) && !nCurrentLevel )
    {
        dummy();
        bDebug = true;
    }
#endif

    // Assume we have no matches on this complete identifier
    //m_bMatchingIdentifier = false;

    // Check if the amout of pathmatches is the same as the level - 1 we have
    for ( uint8_t nQuery = 0; nQuery < this->m_nQueries; nQuery++ )
    {
        // Get the index determined upon initialization time
        nMatchStartPos = this->m_arrPathMatches[ QUERY_DEPTH * nQuery + nCurrentLevel ];

#ifdef DEBUG
        if ( bDebug )
        {
            cout << "\n################ Testing ################" << endl;
            cout << "nQuery        : " << unsigned( nQuery ) << endl;
            cout << "nCurrentLevel : " << unsigned( nCurrentLevel ) << endl;
            cout << "nMatchStartPos: ";
            for ( uint8_t nMatch = 0; nMatch < QUERY_DEPTH; nMatch++ )
            {
                cout << unsigned(this->m_arrPathMatches[ QUERY_DEPTH * nQuery + nMatch ]) << " ";
            }
            cout << endl;
        }
#endif

        // Continue to the next query if the value is already found
        if ( nMatchStartPos == UINT_MAX )
        {
            // If the next is 0: it is a full match (already found), flag
            if ( nCurrentLevel < QUERY_DEPTH && this->m_arrPathMatches[ QUERY_DEPTH * nQuery + nCurrentLevel + 1 ] == 0 )
            {
#ifdef DEBUG
                print( (char*)"Old match\n" );
#endif
                // CompleteFullResult if 'ok' || Complete
                if ( status < ParseStatus::Complete )
                    status = ParseStatus::Complete;
                //m_bMatchingIdentifier = true;
                nMatchCount++;
            }

            continue;
        }

        //
        if ( nCurrentLevel && this->m_arrPathMatches[ QUERY_DEPTH * nQuery + nCurrentLevel - 1 ] != UINT_MAX )
            continue;

        // Current query part is smaller than the identifier length? Skip
        UINT_S nQueryPartLength = strlen( this->m_arrQueries[ nQuery ] + nMatchStartPos );
        if ( nQueryPartLength < nValueLength )
            continue;

        // Current query part is larger than the identifier length? Skip
        if ( ( nQueryPartLength > nValueLength )
          && ( this->m_arrQueries[ nQuery ][ nMatchStartPos + nValueLength ] != '.' )
          && ( this->m_arrQueries[ nQuery ][ nMatchStartPos + nValueLength ] != '[' )
          && ( this->m_arrQueries[ nQuery ][ nMatchStartPos + nValueLength ] != ']' ) )
            continue;

        // Current query part and identifier don't match? Skip
        if ( strncmp( this->m_strValue, this->m_arrQueries[ nQuery ] + nMatchStartPos, nValueLength ) )
            continue;

        // It's a match! Mark the current level(depth) of the current query as found/match
        this->m_arrPathMatches[ QUERY_DEPTH * nQuery + nCurrentLevel ] = UINT_MAX;

        // If the next is 0 (terminator): it is a full match, flag
        if ( nCurrentLevel < QUERY_DEPTH && this->m_arrPathMatches[ QUERY_DEPTH * nQuery + nCurrentLevel + 1 ] == 0 )
        {
#ifdef DEBUG
            //print( (char*)"Full match!\n" );
#endif
#if defined(DEBUG) && defined(INSANE)
            cout << "FULL MATCH ";
            for ( uint8_t nMatch = 0; nMatch < QUERY_DEPTH; nMatch++ )
            {
                cout << unsigned(this->m_arrPathMatches[ QUERY_DEPTH * nQuery + nMatch ]) << " ";
            }
#endif

            status = ParseStatus::CompletePartialResult;

            //m_bMatchingIdentifier = true;
            nMatchCount++;
        }

#ifdef DEBUG
        if ( bDebug )
        {
            cout << "################ after ################" << endl;
            cout << "nQuery        : " << unsigned( nQuery ) << endl;
            cout << "nCurrentLevel : " << unsigned( nCurrentLevel ) << endl;
            cout << "nMatchStartPos: ";
            for ( uint8_t nMatch = 0; nMatch < QUERY_DEPTH; nMatch++ )
            {
                cout << unsigned(this->m_arrPathMatches[ QUERY_DEPTH * nQuery + nMatch ]) << " ";
            }
            cout << endl;
        }
#endif


    }

    // Return full result if we found all matches
    if ( m_nQueries == nMatchCount )
        status = ParseStatus::CompleteFullResult;

#ifdef DEBUG
        if ( bDebug )
        {
            cout << "################ done ################\n" << endl;
        }
#endif
    return status;
};

/**
 * Checks whether the completed value belongs to any of the given matching queries
 * @returns {ParseStatus} 'Ok' if everything went ok, 'AllocationError' if the complete value doesn't fit
 */
ParseStatus JsonVarFetch::_checkCompleteValue( )
{
    UINT_S nMatchStartPos;
    uint8_t nCurrentLevel = this->_getLevel( ) - 1;
    UINT_S nValueLength = strlen( this->m_strValue );

    uint8_t nMatches = 0;

    // No length means: nothing to edit
    if ( !nValueLength )
        return ParseStatus::Ok;

    // Check if the amout of pathmatches is the same as the level - 1 we have
    for ( uint8_t nQuery = 0; nQuery < this->m_nQueries; nQuery++ )
    {
#if defined(DEBUG) && defined(INSANE)
        cout << "Q" << unsigned( nQuery ) << ": ";
#endif

        // Not yet found or already completed
        if ( this->m_arrPathMatches[ QUERY_DEPTH * nQuery + nCurrentLevel ] != UINT_MAX )
        {
#if defined(DEBUG) && defined(INSANE)
            cout << "not found or already completed ";
#endif
            continue;
        }

        // This identifier is not our target: we have to go deeper, so skip for now
        if ( nCurrentLevel < QUERY_DEPTH && this->m_arrPathMatches[ QUERY_DEPTH * nQuery + nCurrentLevel + 1 ] != 0 )
        {
#if defined(DEBUG) && defined(INSANE)
            cout << "not our target ";
#endif
            continue;
        }

#if defined(DEBUG) && defined(INSANE)
        cout << "MATCH [" << this->m_strValue << "] ";
#endif

        // We have a match!
        // Clear the whole query matches so we don't match it again
        for ( uint8_t nPathPart = 0; nPathPart < QUERY_DEPTH/*nCurrentLevel*/; nPathPart++ )
        {
            this->m_arrPathMatches[ QUERY_DEPTH * nQuery + nPathPart ] = 0;
        }

        // Set the nQuery'th string (note that strings are suffixed with a 'zero'; make room if we have to
        UINT_S nStringOffset = 0;
        UINT_S nCurrentResultLength;
        for ( UINT_S nStringIndex = 0; nStringIndex < nQuery; nStringIndex++ )
        {
            nCurrentResultLength = strlen( this->m_strResult + nStringOffset );

            nStringOffset += ( nCurrentResultLength + 1 );
            /* NEW
            nStringOffset += ( strlen( this->m_strResult + nStringOffset ) + 1 );
            */
        }
#if defined(DEBUG) && defined(INSANE)
        cout << "offset " << unsigned( nStringOffset ) << " ";
#endif

        // Check if the current value fits in the given space
        if ( nStringOffset + nValueLength >= this->m_nMaxLen )
        {
#ifdef DEBUG
            print( (char*)"offset + valuelength > maxlen!\n" );
#endif
            return ParseStatus::AllocationError;
        }

        // See if we need to shift stuff around
        // add x, len 7
        // [_yyyyyyyyyy          0]
        // [_yyyyyyyyyy   -------0]
        // [       _yyyyyyyyyy   0]
        // [xxxxxxx_yyyyyyyyyy   0]

        // [_yyyyyyyyyy_zzzzzz   0]
        // [_yyyyyyyyyy_zz!-zz   0]

        // [xxxxxxx_yyyyyyyyyy_zzz]zzz

        // Make sure the last nValueLength bytes are 0
        for ( UINT_S nResult = this->m_nMaxLen - nValueLength; nResult < this->m_nMaxLen; nResult++ )
        {
            if ( this->m_strResult[ nResult ] != 0 )
            {
#ifdef DEBUG
                print( (char*)"end padding not empty!\n" );
#endif
                return ParseStatus::AllocationError;
            }
        }

#if defined(DEBUG) && defined(INSANE)
        cout << "shift " << unsigned( nValueLength ) << " to the right @ " << unsigned( this->m_nMaxLen - nValueLength - 2 ) << " (" << unsigned( this->m_nMaxLen ) << ") ";
#endif

        // Make some room for the current result by shifting the next bytes to the right
        for ( UINT_S nResult = this->m_nMaxLen - nValueLength - 2; nResult >= nStringOffset + 1; nResult-- )
        {
            // Shift the characters
            this->m_strResult[ nResult + nValueLength ] = this->m_strResult[ nResult ];
        }

        // Copy over the value including terminating zero
        memcpy( this->m_strResult + nStringOffset, this->m_strValue, nValueLength + 1 );
    }

#if defined(DEBUG) && defined(INSANE)
    cout << "RESULTS: ";

    nValueLength = 0;
    for ( uint8_t nQuery = 0; nQuery < this->m_nQueries; nQuery++ )
    {
        cout << "[" << this->m_strResult + nValueLength << "] ";
        nValueLength += strlen( this->m_strResult ) + 1;
    }
#endif

    // Success
    // TODO: check the number of results.., return Complete, CompletePartialResult or CompleteFullResult
    //if ( !nMatches )
    return ParseStatus::Ok;
};

#ifdef DEBUG
/**
 * Print debug data and the given _eParserState
 * This includes strValue, iterator, stack level (depth) and parser state size (depth)
 * @param _eParserState {ParserState} The parser state to print
 */
void JsonVarFetch::_printDebugData( ParserState _eParserState )
{
#if defined(INSANE)
    return;
#endif


#if defined(INFO)
    print( (char*)"strValue: [" );
    print( this->m_strValue );
    print( (char*)"]\n" );
#endif

#if defined(INSANE)

    char strNumber[ 10 ];


    print( (char*)"iterator: " );
    itoa( m_nIterator, strNumber, 10 );
    print( strNumber );
    print( (char*)"\n" );

    print( (char*)"level (stack): " );
    itoa( this->_getLevel( ), strNumber, 10 );
    print( strNumber );
    print( (char*)" (" );
    itoa( m_nStateSize, strNumber, 10 );
    print( strNumber );
    print( (char*)")\n" );
#endif

#if defined(VERBOSE)
    print( (char*)"ParserState: " );
    switch ( _eParserState )
    {
        case ParserState::Uninitialized:
            print( (char*)"Uninitialized\n" );
            break;

        case ParserState::Object:
            print( (char*)"Object\n" );
            break;

        case ParserState::Array:
            print( (char*)"Array\n" );
            break;

        case ParserState::Value:
            print( (char*)"Value\n" );
            break;

        case ParserState::String:
            print( (char*)"String\n" );
            break;

        case ParserState::Pair:
            print( (char*)"Pair\n" );
            break;

        case ParserState::Identifier:
            print( (char*)"Identifier\n" );
            break;

        case ParserState::Number:
            print( (char*)"Number\n" );
            break;

        case ParserState::Special:
            print( (char*)"Special\n" );
            break;

        case ParserState::NextSegment:
            print( (char*)"NextSegment\n" );
            break;

        case ParserState::Corrupt:
            print( (char*)"Corrupt\n" );
            break;

        default:
            print( (char*)"Unknown\n" );
            break;
    }

    print( (char*)"Match identifier: " );
    printParseStatus( m_eMatchineIdentifier );
    print( (char*)"\n" );

#endif
};

void JsonVarFetch::printParseStatus( ParseStatus _parseStatus )
{
    switch ( _parseStatus )
    {
        case ParseStatus::AllocationError:
            print( (char*)"AllocationError" );
            break;

        case ParseStatus::ParserError:
            print( (char*)"ParserError" );
            break;

        case ParseStatus::JsonError:
            print( (char*)"JsonError" );
            break;

        case ParseStatus::Ok:
            print( (char*)"Ok" );
            break;

        case ParseStatus::Complete:
            print( (char*)"Complete" );
            break;

        case ParseStatus::CompletePartialResult:
            print( (char*)"CompletePartialResult" );
            break;

        case ParseStatus::CompleteFullResult:
            print( (char*)"CompleteFullResult" );
            break;

        default:
            print( (char*)"Unknown" );
            break;
    }
};

/**
 * Print debug string message (either on Arduino serial or console out)
 * @param _strMessage {char*} The string to print
 */
void JsonVarFetch::print( char* _strMessage )
{
#ifdef ARDUINO
    Serial.print( _strMessage );
#else
    cout << _strMessage;
#endif
};

void JsonVarFetch::dummy()
{
    print( (char*)"Dummy\n" );
}
#endif


