from __future__ import print_function

import requests
import json
import boto3


def build_response_payload( title, output, reprompt_text, should_end_session ):
    return {
        'outputSpeech': {
            'type': 'PlainText',
            'text': output
        },
        'card': {
            'type': 'Simple',
            'title': "SmartLight - " + title,
            'content': "SmartLight - " + output
        },
        'reprompt': {
            'outputSpeech': {
                'type': 'PlainText',
                'text': reprompt_text
            }
        },
        'shouldEndSession': should_end_session
    }
 

def build_response( session_attributes, speechlet_response ):
    return {
        'version': '1.0',
        'sessionAttributes': session_attributes,
        'response': speechlet_response
    }
 

def publish_light(session_arg):
    client = boto3.client('iot-data', region_name='us-east-1')
    response = client.publish(
       topic='SmartLight',
       qos=1,
       payload=json.dumps({"light": session_arg})
    )


def publish_scene(session_arg):
    client = boto3.client('iot-data', region_name='us-east-1')
    response = client.publish(
       topic='SmartLight',
       qos=1,
       payload=json.dumps({"scene": session_arg})
    )


def publish_brightness(session_arg):
    client = boto3.client('iot-data', region_name='us-east-1')
    response = client.publish(
       topic='SmartLight',
       qos=1,
       payload=json.dumps({"brightness": session_arg})
    )


def light_intent(event, context):
    print("=== LIGHT_INTENT ===")
    print("EVENT", event)
    session_attributes = event['request']['intent']['slots']['onoff']['value']
    print("SESSION_ATTRIBUTES:" +  session_attributes)
    print ("=== session_attributes ===", session_attributes)
    publish_light(session_attributes)
    card_title = "Light" + session_attributes
    print ("CARD:", card_title)
    speech_output = "light " + session_attributes
    
    should_end_session = False
    return build_response( session_attributes, build_response_payload( card_title, speech_output, speech_output, should_end_session ) )

def brightness_intent(event, context):
    print("=== SCENE ===")
    print("EVENT", event)

    if event['request']['intent']['slots']['brightness'].get('value'):
        session_attributes = event['request']['intent']['slots']['brightness']['value']
    elif event['request']['intent']['slots']['brighterdimmer'].get('value'):
        session_attributes = event['request']['intent']['slots']['brighterdimmer']['value']

    try:
        n = int(session_attributes)

        if n > 100:
            session_attributes = "100"
        elif n < 0:
            session_attributes = "0"
    except Exception as e:
        pass

    if session_attributes in ["brighter", "higher"]:
        session_attributes = "999"
    elif session_attributes in ["dimmer", "lower"]:
        session_attributes = "-1"

    print("SESSION_ATTRIBUTES:" +  session_attributes)

    print ("=== session_attributes ===", session_attributes)
    publish_brightness(session_attributes)
    card_title = "Brightness" + session_attributes
    print ("CARD:", card_title)

    if session_attributes == "999":
        speech_output = "brightness increased"
    elif session_attributes == "-1":
        speech_output = "brightness decreased"
    else:
        speech_output = "brightness " + session_attributes
    
    should_end_session = False
    return build_response( session_attributes, build_response_payload( card_title, speech_output, speech_output, should_end_session ) )

def scene_intent(event, context):
    print("=== SCENE ===")
    print("EVENT", event)

    scene = event['request']['intent']['slots']['scene']['value']
    
    duration = 0
    unit = ''
    if event['request']['intent']['slots'].get('amount'):
        if event['request']['intent']['slots']['amount'].get('value'):
            duration = int(event['request']['intent']['slots']['amount']['value'])

    if event['request']['intent']['slots'].get('minutehour'):
        if event['request']['intent']['slots']['minutehour'].get('value'):
            unit = event['request']['intent']['slots']['minutehour']['value']

    if unit in ["hour", "hours"]:
        duration = (duration * 60)

    if duration:
        session_attributes = json.dumps({scene: duration})
    else:
        session_attributes = event['request']['intent']['slots']['scene']['value']

    print("SESSION_ATTRIBUTES:" +  session_attributes)

    print ("=== session_attributes ===", session_attributes)
    publish_scene(session_attributes)
    card_title = "Scene" + session_attributes
    print ("CARD:", card_title)

    if duration == 1:
        speech_output = scene  + " playing " + str(duration) + "minute"
    elif duration < 60:
        speech_output = scene  + " playing " + str(duration) + "minutes"
    elif duration >= 60:
        duration = duration/60
        if duration == 1:
            speech_output = scene  + " playing " + str(duration) + "hour"
        else:
            speech_output = scene  + " playing " + str(duration) + "hours"

    should_end_session = False
    return build_response( session_attributes, build_response_payload( card_title, speech_output, speech_output, should_end_session ) )

def say_hello_world():
    session_attributes = {}
    card_title = "SmartLight"
    speech_output = "SmartLight loaded"
    should_end_session = False
    return build_response( session_attributes, build_response_payload( card_title, speech_output, speech_output, should_end_session ) )

##############################
# Required Intents
##############################

def cancel_intent():
    session_attributes = {}
    card_title = "Canceling"
    speech_output = "Canceling.  Goodby."
    should_end_session = True
    return build_response( session_attributes, build_response_payload( card_title, speech_output, speech_output, should_end_session ) )


def help_intent():
    session_attributes = {}
    card_title = "Help"
    speech_output = "You can say turn on or off light,\
                              brighter or dimmer,\
                              play sunrise or sunset,\
                              set light to a percentage,\
                              stop,\
                              cancel"
    should_end_session = False
    return build_response( session_attributes, build_response_payload( card_title, speech_output, speech_output, should_end_session ) )


def stop_intent():
    session_attributes = {}
    card_title = "Stopping."
    speech_output = "Stopping.  Goodby"
    should_end_session = True
    return build_response( session_attributes, build_response_payload( card_title, speech_output, speech_output, should_end_session ) )

##############################
# On Launch
##############################

def on_launch(event, context):
    return statement("title", "body")


##############################
# Routing
##############################

def intent_router(event, context):
    intent = event['request']['intent']['name']

    # Custom Intents

    if intent == "LightIntent":
        return light_intent(event, context)

    if intent == "SceneIntent":
        return scene_intent (event, context)

    if intent == "BrightnessIntent":
        return brightness_intent (event, context)

    # Required Intents

    if intent == "CancelIntent":
        return cancel_intent()

    if intent == "HelpIntent":
        return help_intent()

    if intent == "StopIntent":
        return stop_intent()

# Called when the session starts
def on_session_started(session_started_request, session):
    print("on_session_started requestId=" + session_started_request['requestId'] + ", sessionId=" + session['sessionId'])


def lambda_handler( event, context ):

    session_attributes = {}

    print("event.session.application.applicationId=" + event['session']['application']['applicationId'])

    if event['session']['new']:
        on_session_started({'requestId': event['request']['requestId']}, event['session'])

    if event['request']['type'] == "IntentRequest":
        return intent_router(event, context)
    else:
        return say_hello_world()

if __name__ == '__main__':

    '''
    event = {'request': { 'intent': { 'slots': { 'onoff': { 'value': 'on' } } }}}
    light_intent(event, "")
    event = {'request': { 'intent': { 'slots': { 'onoff': { 'value': 'off' } } }}}
    light_intent(event, "")
    event = {'request': { 'intent': { 'slots': { 'scene': { 'value': 'sunrise' } } }}}
    scene_intent(event, "")

    '''
    event = {'request': { 'intent': { 'slots': { 'scene': { 'value': 'sunset' } } }}}
    scene_intent(event, "")

    event = {'request': { 'intent': { 'slots': { 'scene': { 'value': 'sunset' },  'amount': { 'value': '60' },  'minutehour': { 'value': 'minutes' }, } }}}
    scene_intent(event, "")

    event = {'request': { 'intent': { 'slots': { 'scene': { 'value': 'sunset' },  'amount': { 'value': '2' },  'minutehour': { 'value': 'hours' }, } }}}
    scene_intent(event, "")
    '''

    #event['request']['intent']['slots']['brightness']['value']

    event = {'request': { 'intent': { 'slots': { 'brightness': { 'value': '100'} }  }}}
    brightness_intent(event, "")
    event = {'request': { 'intent': { 'slots': { 'brightness': { 'value': '500'} }  }}}
    brightness_intent(event, "")

    event = {'request': { 'intent': { 'slots': { 'brightness': { 'value': 'brighter' } } }}}
    brightness_intent(event, "")

    event = {'request': { 'intent': { 'slots': { 'brightness': { 'value': 'dimmer' } } }}}
    brightness_intent(event, "")
    '''

