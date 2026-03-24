# TODOS for PR
Things to take care of before creating a PR for the NEC-Module

## ORGA
+ Ask someone from HAW Riot team to review module
+ Return electronics
+ Create offical Pull Request

## Documentation
+ Add README
+ Add comments

## Testing
+ Write a "hello world" example
+ Measure protocoll performance and document results
+ Test if data transmission works properly [✔️]
+ Create atleast one End To End Test [✔️]
+ Review and possibly extend tests unittest [✔️]

## Code
+ Review code according to modern C coding standards and RIOT conventions
+ Declare all funktions as static that are private [✔️]
### Protocol
+ Create proper error handling => check ringbuffer for powers of two!!!
+ Split Logic in receive and send ??? [✔️]
+ Include PWM functions in NEC-Module [✔️]
+ Optimizes statemachine ???

### Memory
+ Replace old message_buffer module with cleaner solution [✔️]
