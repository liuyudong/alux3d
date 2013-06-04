// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#ifndef _TORQUE_UUID_H_
#define _TORQUE_UUID_H_

#ifndef _PLATFORM_H_
   #include "platform/platform.h"
#endif


namespace Torque
{
   /// A universally unique identifier.
   class UUID
   {
      public:
      
         typedef void Parent;
         
      protected:

         U32   a;
         U16   b;
         U16   c;
         U8    d;
         U8    e;
         U8    f[ 6 ];
         
         static UUID smNull;
         
      public:
      
         UUID()
         {
            dMemset( this, 0, sizeof( UUID ) );
         }
         
         ///
         bool isNull() const { return ( *this == smNull ); }
         
         /// Generate a new universally unique identifier (UUID).
         void generate();
         
         /// Convert the given universally unique identifier to a printed string
         /// representation.
         String toString() const;
         
         /// Parse a text string generated by UUIDToString back into a
         /// universally unique identifier (UUID).
         bool fromString( const char* str );

         /// Return a hash value for this UUID.
         U32 getHash() const;

         bool operator ==( const UUID& uuid ) const
         {
            return ( dMemcmp( this, &uuid, sizeof( UUID ) ) == 0 );
         }
         bool operator !=( const UUID& uuid ) const
         {
            return !( *this == uuid );
         }
   };
}

namespace DictHash
{
   inline U32 hash( const Torque::UUID& uuid )
   {
      return uuid.getHash();
   }
}

#endif // !_TORQUE_UUID_H_
