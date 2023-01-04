/** @type {import('./$types').RequestHandler} */
export async function POST() {

    let responseBody = { message: "completed" };
    return new Response(JSON.stringify(responseBody), { status: 200 });
}
