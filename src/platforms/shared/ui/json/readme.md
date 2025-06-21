# Json UI Components

Provides UI Components which emit Json events when they are added and destroyed.

A JsonUIManager will take care of updating internal state.

The JsonUiInternal will have the shared stated. The owning class is responsible
for determining how to parse the Json data and setting it's value.
