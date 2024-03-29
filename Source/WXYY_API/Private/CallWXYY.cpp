// 秦伟杰 版权所有

#include "CallWXYY.h"
#include "Http.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

UCallWXYY *UCallWXYY::CallWXYY(UObject *WorldContextObject, const FString &Token, const TArray<FChatLog> &messages)
{
    // new obj
    UCallWXYY *CallWXYY = NewObject<UCallWXYY>();
    // set obj
    CallWXYY->messages = messages;
    CallWXYY->Token = Token;
    CallWXYY->WorldContextObject = WorldContextObject;
    CallWXYY->RegisterWithGameInstance(WorldContextObject);
    return CallWXYY;
}

void UCallWXYY::Activate()
{
    // Tonke is empty
    if (Token.IsEmpty())
    {
        Finished.Broadcast({}, TEXT("Token is not set"), false);
    }

    // new Http request
    auto HttpRequest = FHttpModule::Get().CreateRequest();
    FString url = TEXT("https://aip.baidubce.com/rpc/2.0/ai_custom/v1/wenxinworkshop/chat/completions?access_token=" + Token);
    HttpRequest->SetURL(url);
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

    TSharedPtr<FJsonObject> HttpBody = MakeShareable(new FJsonObject());

    if (!messages.Num() == 0)
    {
        TArray<TSharedPtr<FJsonValue>> MessagesArray;
        FString role;
        for (int i = 0; i < messages.Num(); i++)
        {
            switch (messages[i].role)
            {
            case EOAChatRole::USER:
                role = TEXT("user");
                break;
            case EOAChatRole::ASSISTANT:
                role = TEXT("assistant");
                break;
            }
            TSharedPtr<FJsonObject> message = MakeShared<FJsonObject>();
            message->SetStringField(TEXT("role"), role);
            message->SetStringField(TEXT("content"), messages[i].content);
            MessagesArray.Add(MakeShareable(new FJsonValueObject(message)));
        }
        HttpBody->SetArrayField(TEXT("messages"), MessagesArray);
    }

    FString Bodystr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Bodystr);
    FJsonSerializer::Serialize(HttpBody.ToSharedRef(), Writer);

    UE_LOG(LogTemp, Warning, TEXT("Bodystr:%s"), *Bodystr);

    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetContentAsString(Bodystr);

    if (HttpRequest->ProcessRequest())
    {
        //TODO 流式响应；

        HttpRequest->OnProcessRequestComplete().BindUObject(this, &UCallWXYY::OnResponse);
    }
    else
    {
        Finished.Broadcast({}, TEXT("Error sending request"), false);
    }
}

void UCallWXYY::OnResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool WasSuccessful)
{
    if (!WasSuccessful)
    {
        UE_LOG(LogTemp, Warning, TEXT("Error processing request. \n%s \n%s"), *Response->GetContentAsString(), *Response->GetURL());
        Finished.Broadcast({}, *Response->GetContentAsString(), false);
    }
    TSharedPtr<FJsonObject> responseObject;
    TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
    if (FJsonSerializer::Deserialize(reader, responseObject))
    {
        bool err = responseObject->HasField(TEXT("error_code"));

        if (err)
        {
            UE_LOG(LogTemp, Warning, TEXT("%s"), *Response->GetContentAsString());
            Finished.Broadcast({}, TEXT("Api error"), false);
            return;
        }
        FChatLog _out;
        _out.role = EOAChatRole::ASSISTANT;
        _out.content = responseObject->GetStringField(TEXT("result"));
        Finished.Broadcast(_out, "", true);
    }
}
